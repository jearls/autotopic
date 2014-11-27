#define PURPLE_PLUGINS

/* standard C include files */

#include <glib.h>
#include <string.h>
#include <stdarg.h>

/*  purple plugin include files  */

#include "cmds.h"
#include "conversation.h"
#include "debug.h"
#include "eventloop.h"
#include "plugin.h"
#include "pluginpref.h"
#include "prefs.h"
#include "signals.h"
#include "version.h"

/*  define my plugin parameters  */

#define PLUGIN_ID "core-jearls-autotopic"
#define PLUGIN_NAME "AutoTopic"
#define PLUGIN_VERSION "v0.2.0-alpha"
#define PLUGIN_AUTHOR "Johnson Earls"
#define PLUGIN_URL "https://github.com/jearls/autotopic/wiki"
#define PLUGIN_SUMMARY "Remembers chatroom topics and automatically sets them when needed."
#define PLUGIN_DESCRIPTION "This plugin allows you to mark chatrooms for which you want to automatically set the topic.  Whenever you join an autotopic chatroom which has no topic, or if you're in an autotopic chatroom and the topic is set to blank, the plugin will automatically set the topic to the last recorded topic for that chatroom."

#define PREFS_ROOT "/plugins/core/" PLUGIN_ID

/* sub-preference names for topic and set-on-join preferences */
#define PREFS_TOPIC "topic"
#define PREFS_SET_ON_JOIN "set_on_buddy_join"

/* the time (in seconds) after joining a chat in which to check the topic */
#define CHAT_JOINED_TOPIC_CHECK_TIMER 5

/* the time (in seconds) after enabling the plugin in which to check the topic for all chats */
#define PLUGIN_LOADED_TOPIC_CHECK_TIMER 1

/* the time (in seconds) after a buddy joins a chat in which to set the topic */
#define CHAT_BUDDY_JOINED_SET_TOPIC_TIMER 1

/* debugging code to write to both debug window and system log ********/

static gboolean debug_to_system_log = FALSE ;

static void
debug_and_log(PurpleAccount *acct, PurpleDebugLevel level, char *cat, char *fmt, ...) {
    va_list args ;
    gchar *arg_s ;
    va_start(args, fmt) ;
    arg_s = g_strdup_vprintf(fmt, args) ;
    va_end(args) ;
    purple_debug(level, cat, "%s", arg_s) ;
    if (debug_to_system_log && (acct != NULL)) {
        gchar *log_s = g_strdup_printf("%s: %s", cat, arg_s) ;
        purple_log_write(purple_account_get_log(acct, TRUE), PURPLE_MESSAGE_SYSTEM, cat, time(NULL), log_s) ;
        g_free(log_s) ;
    }
    g_free(arg_s) ;
}

/* conversation and preference topic handlers *************************/

/*
 *  const char *autotopic_get_topic(PurpleConversation *conv)
 *  Returns the topic set in the preferences for the indicated conversation.
 *  If the conversation is watched, but no topic is set, return a pointer to an empty string.
 *  If the conversation is not watched, return NULL.
 */

static const char *
autotopic_get_topic(PurpleConversation *conv) {
    const char *topic ;
    const char *name ;
    gchar *pref_name ;
    name = purple_conversation_get_name(conv) ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_get_topic: conversation=\"%s\"\n", name ) ;
    topic = NULL ;
    pref_name = g_strdup_printf("%s/%s/%s", PREFS_ROOT, name, PREFS_TOPIC) ;
    if (purple_prefs_exists(pref_name)) {
        topic = purple_prefs_get_string(pref_name) ;
    }
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_get_topic: pref \"%s\" -> %s%s%s\n", pref_name, (topic ? "\"" : ""), (topic ? topic : "NULL"), (topic ? "\"" : "")) ;
    g_free(pref_name) ;
    return topic ;
}

/*
 *  gboolean autotopic_get_set_on_join(PurpleConversation *conv)
 *  Returns the set_on_join preference for the indicated conversation.
 *  If the conversation is not watched, return FALSE.
 */

static gboolean
autotopic_get_set_on_join(PurpleConversation *conv) {
    gboolean set_on_join = FALSE ;
    const char *name ;
    gchar *pref_name ;
    name = purple_conversation_get_name(conv) ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_get_topic: conversation=\"%s\"\n", name ) ;
    pref_name = g_strdup_printf("%s/%s/%s", PREFS_ROOT, name, PREFS_SET_ON_JOIN) ;
    if (purple_prefs_exists(pref_name)) {
        set_on_join = purple_prefs_get_bool(pref_name) ;
    }
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_get_topic: pref \"%s\" -> %s\n", pref_name, (set_on_join ? "TRUE" : "FALSE")) ;
    g_free(pref_name) ;
    return set_on_join ;
}

/*
 *  void autotopic_set_topic(PurpleConversation *conv, const char *topic)
 *  Sets the preferences topic for the given conversation to <topic>
 */

static void
autotopic_set_topic(PurpleConversation *conv, const char *topic) {
    const char *name;
    gchar *chatroom_pref, *topic_pref, *set_on_join_pref ;
    name = purple_conversation_get_name(conv) ;
    if (topic == NULL) {
        topic = "" ;
    }
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_set_topic: conversation = \"%s\", topic=\"%s\"\n", name, topic) ;
    chatroom_pref = g_strdup_printf("%s/%s", PREFS_ROOT, name) ;
    topic_pref = g_strdup_printf("%s/%s", chatroom_pref, PREFS_TOPIC) ;
    set_on_join_pref = g_strdup_printf("%s/%s", chatroom_pref, PREFS_SET_ON_JOIN) ;
    if (!purple_prefs_exists(chatroom_pref)) {
        purple_prefs_add_none(chatroom_pref) ;
    }
    if (!purple_prefs_exists(topic_pref)) {
        purple_prefs_add_string(topic_pref, NULL) ;
    }
    if (!purple_prefs_exists(set_on_join_pref)) {
        purple_prefs_add_bool(set_on_join_pref, FALSE) ;
    }
    purple_prefs_set_string(topic_pref, topic) ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_set_topic: pref \"%s\" -> \"%s\"\n", topic_pref, topic) ;
    g_free(set_on_join_pref) ;
    g_free(topic_pref) ;
    g_free(chatroom_pref) ;

    return ;
}

/*
 *  void autotopic_set_set_on_join(PurpleConversation *conv, gboolean set_on_join)
 *  Sets the set_on_join preference for the given conversation to
 *  <set_on_join>.
 */

static void
autotopic_set_set_on_join(PurpleConversation *conv, gboolean set_on_join) {
    const char *name;
    gchar *chatroom_pref, *topic_pref, *set_on_join_pref ;
    name = purple_conversation_get_name(conv) ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_set_set_on_join: conversation = \"%s\"\n", name) ;
    chatroom_pref = g_strdup_printf("%s/%s", PREFS_ROOT, name) ;
    topic_pref = g_strdup_printf("%s/%s", chatroom_pref, PREFS_TOPIC) ;
    set_on_join_pref = g_strdup_printf("%s/%s", chatroom_pref, PREFS_SET_ON_JOIN) ;
    if (!purple_prefs_exists(chatroom_pref)) {
        purple_prefs_add_none(chatroom_pref) ;
    }
    if (!purple_prefs_exists(topic_pref)) {
        purple_prefs_add_string(topic_pref, purple_conv_chat_get_topic(purple_conversation_get_chat_data(conv))) ;
    }
    if (!purple_prefs_exists(set_on_join_pref)) {
        purple_prefs_add_bool(set_on_join_pref, !set_on_join) ;
    }
    purple_prefs_set_bool(set_on_join_pref, set_on_join) ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_set_set_on_join: pref \"%s\" -> %s\n", set_on_join_pref, (set_on_join ? "TRUE" : "FALSE")) ;
    g_free(set_on_join_pref) ;
    g_free(topic_pref) ;
    g_free(chatroom_pref) ;
}

/*
 *  void autotopic_remove_topic(PurpleConversation *conv)
 *  Removes the preferences topic for the conversation.  This has the
 *  effect of turning off autotopic for the conversation.
 */

static void
autotopic_remove_topic(PurpleConversation *conv) {
    const char *name = purple_conversation_get_name(conv) ;
    gchar *chatroom_pref = g_strdup_printf("%s/%s", PREFS_ROOT, name) ;
    gchar *topic_pref = g_strdup_printf("%s/%s", chatroom_pref, PREFS_TOPIC) ;
    gchar *set_on_join_pref = g_strdup_printf("%s/%s", chatroom_pref, PREFS_SET_ON_JOIN) ;
    /*
     *  work around bug: remove does not schedule preferences save.
     *  set the preference to NULL first, to force a save to be
     *  scheduled, then remove it.
     */
    purple_prefs_set_string(topic_pref, NULL) ;
    purple_prefs_remove(topic_pref) ;
    purple_prefs_remove(set_on_join_pref) ;
    purple_prefs_remove(chatroom_pref) ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_remove_topic: pref \"%s\" -> XX\n", chatroom_pref) ;
    g_free(topic_pref) ;
    g_free(set_on_join_pref) ;
    g_free(chatroom_pref) ;
    return ;
}

/*
 *  void autotopic_send_topic_change(PurpleConversation *conv)
 *  If the chatroom has autotopic enabled, set the topic no matter
 *  what the current topic is.
 */

static void
autotopic_send_topic_change(PurpleConversation *conv) {
    const char *topic_for_chat;
    topic_for_chat = autotopic_get_topic(conv) ;
    if (topic_for_chat != NULL && topic_for_chat[0] != '\0') {
        gchar *set_topic_error = NULL ;
        gchar *cmdbuf ;
        debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "Setting topic to \"%s\".\n", topic_for_chat) ;
        cmdbuf = g_strdup_printf("topic %s", topic_for_chat) ;
        purple_cmd_do_command(conv, cmdbuf, cmdbuf, &set_topic_error) ;
        g_free(cmdbuf) ;
        if (set_topic_error != NULL) {
            cmdbuf = g_strdup_printf("Error setting topic: %s", set_topic_error) ;
            g_free(set_topic_error) ;
            purple_conversation_write(conv, NULL, cmdbuf, PURPLE_MESSAGE_ERROR, time(NULL)) ;
            g_free(cmdbuf) ;
        }
    }
}

/*
 *  void autotopic_handle_topic_change(PurpleConversation *conv, const char *topic)
 *  Handle a conversation change.  If the chatroom has autotopic enabled,
 *  then either change the saved topic to the new chatroom topic, or
 *  (if the new chatroom topic is blank) set the chatroom topic to the
 *  saved topic.
 */

static void autotopic_handle_topic_change(PurpleConversation *conv, const char *new_topic) {
    const char *topic_for_chat;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic_handle_topic_change: conversation=\"%s\" new_topic=\"%s\"\n", purple_conversation_get_name(conv), new_topic) ;
    topic_for_chat = autotopic_get_topic(conv) ;
    if (topic_for_chat != NULL) {
        if ((new_topic == NULL) || (new_topic[0] == '\0')) {
            autotopic_send_topic_change(conv) ;
        } else {
            autotopic_set_topic(conv, new_topic) ;
        }
    }
    return ;
}

/* callback functions *************************************************/

/*
 *  chat_topic_changed_cb - handle when a chat's topic changes
 *  call the topic change handler.
 */

static void
chat_topic_changed_cb(PurpleConversation *conv, const char *who, const char *topic, void *data) {
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "Topic changed: who=\"%s\" account username=\"%s\" topic=\"%s\".\n", who, purple_account_get_name_for_display(purple_conversation_get_account(conv)), topic) ;
    autotopic_handle_topic_change(conv, topic) ;
    return ;
}

/*
 *  check_topic_cb - timer callback to check the topic of a chatroom
 */

static gboolean
check_topic_cb(gpointer user_data) {
    PurpleConversation *conv = (PurpleConversation*)user_data ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "Check Topic callback: conversation=\"%s\".\n", purple_conversation_get_name(conv) ) ;
    autotopic_handle_topic_change(conv, purple_conv_chat_get_topic(purple_conversation_get_chat_data(conv))) ;
    /* return FALSE to stop the timer from calling the callback again */
    return FALSE ;
}

/*
 *  chat_joined_cb - handle joining a chat.
 *  set a timer to call the check_topic callback.
 */

static void
chat_joined_cb(PurpleConversation *conv, void *data) {
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "Chat Joined callback: conversation=\"%s\".\n", purple_conversation_get_name(conv) ) ;
    purple_timeout_add_seconds(
            CHAT_JOINED_TOPIC_CHECK_TIMER,
            (GSourceFunc)check_topic_cb,
            (gpointer)conv
    ) ;
    return ;
}

/*
 *  timer callback hash table used by set_topic_cb and chat_buddy_joined_cb
 */

static GHashTable *timer_hash = NULL ;

/*
 *  set_topic_cb - timer callback to forcibly set the topic of a chatroom
 *  removes the chat room name from the timer callback hash table
 */

static gboolean
set_topic_cb(gpointer user_data) {
    PurpleConversation *conv = (PurpleConversation*)user_data ;
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "Set Topic callback: conversation=\"%s\".\n", purple_conversation_get_name(conv) ) ;
    /*
     *  make sure timer_hash exists and our conversation is in it.
     *  if so, remove it from the hash table and send the topic change.
     */
    if ((timer_hash != NULL) &&
            (g_hash_table_lookup(timer_hash, conv) != NULL)
    ) {
        g_hash_table_remove(timer_hash, conv) ;
        autotopic_send_topic_change(conv) ;
    }
    /* return FALSE to stop the timer from calling the callback again */
    return FALSE ;
}

/*
 *  chat_buddy_joined_cb - handle joining a chat.
 *  set a timer to call the set_topic callback.
 *  maintain a hash table of timer callbacks hashed on the conversation pointer,
 *  to avoid setting the topic multiple times
 */

static void
chat_buddy_joined_cb(PurpleConversation *conv, const char *name, PurpleConvChatBuddyFlags flags, gboolean new_arrival, void *data) {
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "Chat Buddy Joined callback: conversation=\"%s\" buddy=\"%s\" flags=0x%X, new_arrival=%d.\n", purple_conversation_get_name(conv), name, flags, new_arrival ) ;
    /*  initialize timer_hash if needed.  */
    if (timer_hash == NULL) {
        timer_hash = g_hash_table_new(NULL, NULL) ;
    }
    /*
     *  only add the timer if:
     *    the joiner is a new arrival,
     *    the conversation's preference is to set the topic on joins,
     *    and the conversation does not already exist in the hash table.
     */
    if (new_arrival &&
            autotopic_get_set_on_join(conv) &&
            (g_hash_table_lookup(timer_hash, conv) == NULL)
    ) {
        /*  add the conversation to the hash table  */
        g_hash_table_insert(timer_hash, conv, conv) ;
        purple_timeout_add_seconds(
                CHAT_BUDDY_JOINED_SET_TOPIC_TIMER,
                (GSourceFunc)set_topic_cb,
                (gpointer)conv
        ) ;
    }
    return ;
}

/*
 *  check_all_chats - called on plugin load
 *  check all current chats to see if we need to set the topic or
 *  register a topic change
 */

static void
check_all_chats() {
    GList *chat_list ;
    for (
            chat_list = purple_get_chats() ;
            chat_list != NULL ;
            chat_list = chat_list -> next
    ) {
        PurpleConversation *chat = (PurpleConversation *)chat_list -> data ;
        purple_timeout_add_seconds(
                PLUGIN_LOADED_TOPIC_CHECK_TIMER,
                (GSourceFunc)check_topic_cb,
                (gpointer)chat
        ) ;
    }
}

/*
 *  autotopic_cmd_cb - the autotopic command handler.
 *  handles the commands:
 *    /autotopic on
 *      turns on autotopic for the current [chat] conversation
 *    /autotopic off
 *      turns off autotopic for the current [chat] conversation
 *    /autotopic status
 *      reports whether autotopic is turned on or off for the current [chat] conversation
 */

static PurpleCmdId autotopic_cmd_id = 0;
/* the autotopic command word */
#define AUTOTOPIC_CMD_WORD "autotopic"
/* the arguments to the autotopic command:  one word with no formatting */
#define AUTOTOPIC_CMD_ARGS "w"
/* the priority of the autotopic command: plugin default */
#define AUTOTOPIC_CMD_PRI PURPLE_CMD_P_PLUGIN
/* the autotopic command flags: chatroom command */
#define AUTOTOPIC_CMD_FLAGS PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS
/* the protocol of the autotopic command: used only when flags includes PURPLE_CMD_FLAG_PRPL */
#define AUTOTOPIC_CMD_PROTO NULL
/* the autotopic command callback */
#define AUTOTOPIC_CMD_CB PURPLE_CMD_FUNC(autotopic_cmd_cb)
/* the autotopic command help string */
#define AUTOTOPIC_CMD_HELP "autotopic on|off:  turn autotopic on or off for the current chatroom.\n\
autotopic status:  report the status of the current chatroom.\n\
autotopic join|nojoin:  turn on or off setting the topic when new users join the chatroom (implies \"autotopic on\" as well)."

static PurpleCmdRet autotopic_cmd_cb(PurpleConversation *conv,
                              const gchar* cmd,
                              gchar **args,
                              gchar **error,
                              void *data) {
    PurpleCmdRet ret = PURPLE_CMD_RET_OK ;
    gchar *msg = NULL ;
    *error = NULL ;
    /* check arguments. */
    debug_and_log(purple_conversation_get_account(conv), PURPLE_DEBUG_INFO, PLUGIN_ID, "autotopic option %s%s%s.\n", ((args && args[0]) ? "\"" : "") , ((args && args[0]) ? args[0] : "NULL") , ((args && args[0]) ? "\"" : "") ) ;
    /* were we given too many arguments? */
    if (args && args[0] && args[1]) {
        *error = g_strdup_printf("Too many arguments to the autotopic command.") ;
        ret = PURPLE_CMD_RET_FAILED ;
    /* if no arguments, or argument is "status", report status. */
    } else if ((args == NULL) || (args[0] == NULL) || (strcmp(args[0], "status") == 0)) {
        const char *topic_for_chat = autotopic_get_topic(conv) ;
        gboolean set_on_join = autotopic_get_set_on_join(conv) ;
        if (topic_for_chat == NULL) {
            msg = g_strdup_printf("autotopic is off for this chat.") ;
        } else if (set_on_join) {
            msg = g_strdup_printf("autotopic is on for this chat and will set the topic when new users join.") ;
        } else {
            msg = g_strdup_printf("autotopic is on for this chat.") ;
        }
    /* if argument is "on", turn on autotopic. */
    } else if (strcmp(args[0], "on") == 0) {
        const char *topic = purple_conv_chat_get_topic(purple_conversation_get_chat_data(conv)) ;
        autotopic_set_topic(conv, topic) ;
        msg = g_strdup_printf("autotopic is now on for this chat.") ;
    /* if argument is "off", turn off autotopic. */
    } else if (strcmp(args[0], "off") == 0) {
        autotopic_remove_topic(conv) ;
        msg = g_strdup_printf("autotopic is now off for this chat.") ;
    /* if argument is "join", turn on autotopic and set set_on_joined to TRUE. */
    } else if (strcmp(args[0], "join") == 0) {
        autotopic_set_set_on_join(conv, TRUE) ;
        msg = g_strdup_printf("autotopic will set the topic when new users join this chat.") ;
    /* if argument is "nojoin", turn on autotopic and set set_on_joined to FALSE. */
    } else if (strcmp(args[0], "nojoin") == 0) {
        autotopic_set_set_on_join(conv, FALSE) ;
        msg = g_strdup_printf("autotopic will NOT set the topic when new users join this chat.") ;
    /* otherwise, invalid argument... */
    } else {
        *error = g_strdup_printf("Invalid autotopic option \"%s\"", args[0]) ;
        ret = PURPLE_CMD_RET_FAILED ;
    }
    if (msg != NULL) {
        purple_conversation_write(conv, NULL, msg, PURPLE_MESSAGE_SYSTEM, time(NULL)) ;
        g_free(msg) ;
    }
    return ret ;
}

/*  Registers any custom commands provided by the plugin.
 *  Command /autotopic [on|off]
 *    Turns autotopic on or off for the current chatroom.
 */
static void
register_cmds(PurplePlugin *plugin) {
    /* register autotopic command */
    autotopic_cmd_id = purple_cmd_register(
        AUTOTOPIC_CMD_WORD ,
        AUTOTOPIC_CMD_ARGS ,
        AUTOTOPIC_CMD_PRI ,
        AUTOTOPIC_CMD_FLAGS ,
        AUTOTOPIC_CMD_PROTO ,
        AUTOTOPIC_CMD_CB ,
        AUTOTOPIC_CMD_HELP ,
        NULL /* user data to pass */
    );
    /*  Done, nothing to return  */
    return ;
}

/*  Connects signal handlers used by the plugin.
 *  Currently, this consists of:
 *    chat-joined
 *    chat-topic-changed
 */
static void
connect_signals(PurplePlugin *plugin) {
    /*  The conversation handle is used for conversation-related signals  */
    void *conv_handle = purple_conversations_get_handle() ;
    purple_signal_connect(conv_handle, "chat-joined", plugin, PURPLE_CALLBACK(chat_joined_cb), NULL) ;
    purple_signal_connect(conv_handle, "chat-topic-changed", plugin, PURPLE_CALLBACK(chat_topic_changed_cb), NULL) ;
    purple_signal_connect(conv_handle, "chat-buddy-joined", plugin, PURPLE_CALLBACK(chat_buddy_joined_cb), NULL) ;
    /*  Done, nothing to return  */
    return ;
}

/*  Initialize the plugin preferences.
 *  Create the root preference directory if needed.
 *  If there are chatrooms that have direct content (v0.1 preferences),
 *    convert them to v0.2 by moving the content to <chatroom>/topic
 *    and setting <chatroom>/change_on_join to false
 */
static void
init_prefs(PurplePlugin *plugin) {
    GList *children_list ;
    /*  If the root preference directory does not exist, create it  */
    if (!purple_prefs_exists(PREFS_ROOT)) {
        purple_prefs_add_none(PREFS_ROOT) ;
    }
    /*  convert v0.1 preferences if needed  */
    children_list = purple_prefs_get_children_names(PREFS_ROOT) ;
    if (children_list != NULL) {
        GList *child_ptr ;
        for (
                child_ptr = children_list ;
                child_ptr != NULL ;
                child_ptr = child_ptr -> next
        ) {
            char *child_pref = (char*)(child_ptr -> data) ;
            if (purple_prefs_get_type(child_pref) == PURPLE_PREF_STRING) {
                /*
                 *  we have a v0.1 preference; get its value, erase it,
                 *  and recreate using v0.2 format:
                 *  <chatroom>/topic = <string>
                 *  <chatroom>/set_on_buddy_join = <boolean>
                 */
                gchar *child_val = g_strdup(purple_prefs_get_string(child_pref)) ;
                gchar *new_pref = NULL ;
                /*  remove and recreate the base preference  */
                purple_prefs_remove(child_pref) ;
                purple_prefs_add_none(child_pref) ;
                /*  create the set_on_buddy_join preference  */
                new_pref = g_strdup_printf("%s/%s", child_pref, PREFS_SET_ON_JOIN) ;
                purple_prefs_add_bool(new_pref, FALSE) ;
                g_free(new_pref) ;
                /*  create the topic preference  */
                new_pref = g_strdup_printf("%s/%s", child_pref, PREFS_TOPIC) ;
                /*
                 *  work around libpurple preferences bug:
                 *  add NULL, then set the value.
                 *  this forces a preferences save.
                 */
                purple_prefs_add_string(new_pref, NULL) ;
                purple_prefs_set_string(new_pref, child_val) ;
                g_free(child_val) ;
                g_free(new_pref) ;
            }
            g_free(child_pref) ;
        }
        g_list_free(children_list) ;
    }
    /*  Done, nothing to return  */
    return ;
}

/*  Called by the plugin system the first time the plugin is probed.
 *  Does any one-time initialization for the plugin.
 */
static void
init_plugin_hook(PurplePlugin *plugin) {
    debug_and_log(NULL, PURPLE_DEBUG_INFO, PLUGIN_ID, "Plugin Initialized.\n") ;
    /*  Initialize the plugin's preferences  */
    init_prefs(plugin) ;
    /*  Done, nothing to return  */
    return ;
}

/*  Called by the plugin system when the plugin is loaded.
 *  Registers plugin commands and connects signal handlers.
 */
static gboolean
plugin_load_hook(PurplePlugin *plugin) {
    debug_and_log(NULL, PURPLE_DEBUG_INFO, PLUGIN_ID, "Plugin Loaded.\n") ;
    /*  register any custom plugin commands  */
    register_cmds(plugin) ;
    /*  register any signal handlers  */
    connect_signals(plugin) ;
    /*  check any current chats for topic changes  */
    check_all_chats() ;
    /*  return TRUE says continue loading the plugin  */
    return TRUE ;
}

/*  Unload the plugin.
 *  Called by the plugin system when the plugin is unloaded.
 *  Currently does nothing.
 */
static gboolean
plugin_unload_hook(PurplePlugin *plugin) {
    debug_and_log(NULL, PURPLE_DEBUG_INFO, PLUGIN_ID, "Plugin Unloaded.\n") ;
    /*  We don't need to do anything special here  */
    /*  return TRUE says continue unloading the plugin  */
    return TRUE ;
}

/*  The plugin information block.
 *  This plugin uses the following hooks and info blocks:
 *    plugin_load_hook
 *    plugin_unload_hook
 *    plugin_prefs_info
 */
static PurplePluginInfo plugin_info = {
    /* magic */                 PURPLE_PLUGIN_MAGIC ,
    /* compat major version */  PURPLE_MAJOR_VERSION ,
    /* compat minor version */  PURPLE_MINOR_VERSION ,
    /* plugin type */           PURPLE_PLUGIN_STANDARD ,
    /* reserved for ui */       NULL ,
    /* plugin flags */          0 ,
    /* reserved for deps */     NULL ,
    /* priority */              PURPLE_PRIORITY_DEFAULT ,

    /* plugin id */             PLUGIN_ID ,
    /* plugin name */           PLUGIN_NAME ,
    /* plugin version */        PLUGIN_VERSION ,

    /* summary */               PLUGIN_SUMMARY ,
    /* description */           PLUGIN_DESCRIPTION ,
    /* author */                PLUGIN_AUTHOR ,
    /* URL */                   PLUGIN_URL ,

    /* plugin_load */           &plugin_load_hook ,
    /* plugin_unload */         &plugin_unload_hook ,
    /* plugin_destroy */        NULL ,

    /* ui info */               NULL ,
    /* loader/protocol info */  NULL ,
    /* prefs info */            NULL ,
    /* plugin_actions */        NULL ,
    /* reserved */              NULL ,
    /* reserved */              NULL ,
    /* reserved */              NULL ,
    /* reserved */              NULL
} ;

/*  Initialize the plugin.
 *
 *  The first parameter to PURPLE_INIT_PLUGIN (autotopic) is the plugin
 *  name.  This is not a C string, nor is it a declared type or variable.
 *  With PURPLE_PLUGINS defined, this argument is ignored.
 *  If PURPLE_PLUGINS is not defined, this argument is used by the
 *  PURPLE_INIT_PLUGIN as part of some internally-defined function names
 *  (e.g. purple_init_autotopic_plugin).
 *
 *  The second parameter is the plugin initialization function.
 *  The third parameter is the plugin info block.
 */
PURPLE_INIT_PLUGIN(autotopic, init_plugin_hook, plugin_info)
