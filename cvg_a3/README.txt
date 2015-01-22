Assignment 3
CMPT 431
Costin Ghiocel
301027183

[Warning!!!]

- the logs are deleted on SIGINT, only use sigint when you are planning on starting fresh and on BOTH servers!!!!
- both servers use the log for integrity checks, if one has it's log deleted and the other does not, all hell breaks loose.
- Once again, if one server deletes its log file, it's extremely important the other server has its log file deleted (stored in local working folder as .server.log)
- integrity check goes both ways, if each server contains different commits, they will get stuck in a loop, and i did not have time to handle it..
	ie: primary contains tsn: 1 2 ; backup: 2 3;
	each will think it has the most recent and promote itself to backup and demote the other..
	This should generally not happen unless you really force it. The servers have 2 sync methods, this will happen if both fail.
- integrity check will fix things like -- crash both, and 1 had a few extra commits that never got sync and crashed, ie: backup has commits primary does not, it will fix this. Vice-versa as well.

[INFO]:
=================================================================================

* [TM] stands for transaction manager
	* transaction manager manages timeouts and so on, messages
	  will appear when something is deleted
	* expired transactions are still known for a while and server
	  will notify on them but ignore it
* [RM] stands for REPLICATION MANAGER
	* [IMPORTANT NOTE]: please close backup using SIGKILL or similar,
	  SIGINT will cause it to delete log and that's bad.
	  Backup and Primary might be out of sync as a result
	* if server is running as backup, it will resolve primary and send
	  the primary the ip and port for it (the backup), so resolving
	  primary/backup is done automatically.
	* the primary will wait for a backup to register, and attempt to
	  connect to it, it will be verbose about it
	* RM will display messages on syncs and the response it got (if enabled)
	* if the backup disconnects, it will notify and store a stack of
	  messages that need to be sent, which are synced when the backup registers
	* if there is no backup registered, it stores messages and sends
	  them when backup registers, a warning will appear on commit
	* if there is a backup server available, transactions on primary will only
	  be committed after the backup commits. In other words, i guarantee that if
	  the backup fails to commit, the primary will not and return an error.
* Both servers behave as in assignment 2, you can continue a transaction
  after a failure, keeps track of commits etc..
* the backup will be at most 0.3s behind on writes as they are bundled and sent 
  periodically by the daemon.
* a commit will force a sync between servers.
* I added a large number of safeties, but things can still go wrong given wrong 
  operation. 
* i wanted to make the backup refuse connection from anything but primary, but
  that would require hardcoding some stuff, and i felt uncomfortable as it might
  not work in CSIL, instead, you get a pretty reply saying you're not authorised.
==================================================================================

How to use it:
==============

>make
>./Server -[settings] +[flags]


-[settings]:
============

-i [ip] : specify ip of server -- default 127.0.0.1
-p [port] : specify port for server -- default 8080 
-d [dir] : local diretory for server to store files and log
	ie: /home/documents/server1
-n [net_dir] : network location of primary.txt (just the directory! the file name is hardcoded)
	ie: /home/documents
-m : by appending -m (for main) the server will start as primary, there are no safeties for more than 1 primary, so be careful with that.
	no -m will tell the server it is a backup, it will try to load primary.txt, resolfve primary, and register with it.
	if there's no primary.txt file, it will give an error and not start. If the file has an outdated server, it will attempt to connect to it a few times and promote itself as primary if it cannot.

!NOTE: in the case: server used to be backup, updates itself to primary and writes to primary.txt, is started as backup after killed. It will load primary.txt and the info in it will point to itself. It's an easy oversight, so i added a safety.

- if the server gets stuck, it's because it connected to itself, it will enter a loop and async handlers stop being created to prevent other issues. I did my best to make sure this can't happen, but murphy's law states otherwise.

+[flags] -- mostly debug
========================

+info : print info message posted above
+drm  : this tells it to print debug messages for the 
	replication manager daemon, this includes responses 
	from the back/main server, sync requests, etc.. 
+dc   : debug messages from the client (ie: the part that connects to other servers)
	will give reasons why connection failed, etc.

I reccommned using the 2 debug flags since it makes it much easier to understand what's going on.

Example usage:
==============

for Primary:

./Server -p 8080 -d /home/test/server1 -n /home/test -m +drm +dc

for backup:

./Server -p 8060 -d /home/test/server2 -n /home/test +drm +dc


