A GameSpy EncType 0 and EncType 1 emulator.
Based off of Q2Master v1.1 by QwazyWabbit.
Probable exploits, if you find some serious issues email me
emoaddict15@gmail.com.

Supported Games
- Blood 2
- Daikatana
- GameSpy 3D
- GameSpy Lite Utility (used by Kingpin and South Park)
- Heretic 2
- Hexen 2
- HexenWorld
- Kingpin
- No One Lives Forever
- No One Lives Forever 2
- Quake 1
- QuakeSpy Utility
- QuakeWorld
- Quake 2
- Shogo
- SiN
- South Park
- Turok 2
- Unreal 1
- Unreal Tournament 99

This master server code is used in Daikatana v1.3, QDOS, QWDOS, and Q2DOS
for their server browsers.

TCP Port 28900 and UDP Port 27900 must be open.  You may have to open
additional ports for the games you wish to support.  For example most
Quake 2 servers use port 27910.  Daikatana uses two ports, 27992 for the
connection and 27982 for the GameSpy query.  Other games may use this
port scheme.

Commands
 -sendack - by default GameSpy doesn't not send this type of packet out.
            if you want to extend the courtesy of acknowleding the
            heartbeat then enable this setting.

 -quickvalidate - by default the master server requires 1 extra heartbeat
                  and a successful ping request to be added to the query
                  list.  Set this to allow any new server to show up
                  immediately.


 -validationrequired <1, 2, or 3> - Require validation from the challenge key
                                    cross-checked with the games secure key.
                                    1 - client list requests only.
                                    2 - servers.
                                    3 - clients and servers (recommended).

 -timestamp <1 or 2> - Debug outputs are timestampped.  1 - for AM/PM.
                       2 for military.

 -heartbeatinterval <time in minutes> - Time in minutes for sending heartbeats.
                                        Must be at least 1 minute.

 -minimumheartbeats x - Minimum number of sucessful heartbeats that need to be
                        sent before a server will be added to the list.

 -tcpport <port> - Causes server to bind to a particular TCP port for the
                   GameSpy list query from clients. Default is 28900.
                   If you depart from this you need to communicate this to your
                   users somehow.

 -serverlist <filename> - Adds servers from a list.  Hostnames are supported.
  Format is <ip>,<query port>,<gamename> i.e. maraakate.org,27982,daikatana.

 -httpenable - grabs HTTP lists of QW, Q2, and Q1 from QTracker and other
               places.

 -masterlist <filename> - Adds master servers from a list.  Every hour
                          GSMaster will ping these servers to grab their
                          lists. Hostnames are supported.
  Format is <ip>,<query port>,<gamename> i.e. maraakate.org,27900,quakeworld.

 -cwd <path> - Sets the current working directory.  Useful for
               running as a windows service or scheduled task.

 -generateserverlist <folderpath> - Generate a server list as a text file to
                                    the specified folder path.

 -serverlisttimer <seconds> - Time (in seconds) to generate a server list.
                              Use with -generateserverlist.

Known Bugs
- DJGPP port is currently broken.  Compiles and executes fine, but only
pings the last server in a large batch.

Credits
- [HCI]Mara'akate - Code
- CHC - Help with Encode Type 1 encoder
- Daniel Gibson - DG_misc.h string header
- Luigi Auriemma - For GameSpy validation algorithms and Encode Type 1 packet dumps for analysis (aluigi.org)
- QwazyWabbit - Original Code for Q2Master
- Sezero - Portable SOCKET defines, HexenWorld query, and other small code snips
- ID Software - Quake 2 Info_ValueForKeys and other small helper functions
