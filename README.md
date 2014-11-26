AutoTopic
=========

The AutoTopic plugin remembers topics for chatrooms and re-sets the topic for those chatrooms if the topic gets un-set.

Installation
============

Download the `autotopic.dll` file into a temporary location, then install it into either your personal plugins directory or the system-wide plugins directory.

Installing into personal plugins directory
------------------------------------------

Open the Windows Explorer to your Application Data folder: Go to the Start menu, enter `%appdata%` into the search field, and press Return. You should see a `.purple` folder; open that. Create the `plugins` folder if necessary. Finally, copy the `autotopic.dll` file from the download location into the plugins folder.

Installing into the system-wide plugins directory
-------------------------------------------------

Open the Windows Explorer and navigate to `C:\Program Files (x86)\Pidgin\plugins` . Copy the `autotopic.dll` file from the download location into the plugins folder.

Usage
=====

Launch Pidgin and select the Tools → Plugins menu option. Locate and enable the AutoTopic plugin.  This will enable the `/autotopic` command in chat rooms.

In any chat room where you want AutoTopic to remember the topic, type the command `/autotopic on`.

`/autotopic off` will turn AutoTopic off for the chat room.  `/autotopic status` will tell you if AutoTopic is enabled or not.

When AutoTopic is enabled for a chat room, it will remember any topic set for that chat room.  If the topic ever gets un-set, or if you enter the chat room and there is no topic set, AutoTopic will automatically set the topic again to its last remembered topic for that chat room.
