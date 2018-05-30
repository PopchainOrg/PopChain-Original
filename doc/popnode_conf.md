Popnode config
=======================

Pop Core allows controlling multiple remote popnodes from a single wallet. The wallet needs to have a valid collateral output of 10000 coins for each popnode and uses a configuration file named `popnode.conf` which can be found in the following data directory (depending on your operating system):
 * Windows: %APPDATA%\PopCore\
 * Mac OS: ~/Library/Application Support/PopCore/
 * Unix/Linux: ~/.popcore/

`popnode.conf` is a space separated text file. Each line consists of an alias, IP address followed by port, popnode private key, collateral output transaction id and collateral output index.

Example:
pn1 127.0.0.2:19888 93HaYBVUCYjEMeeH1Y4sBGLALQZE1Yc1K64xiqgX37tGBDQL8Xg 7603c20a05258c208b58b0a0d77603b9fc93d47cfa403035f87f3ce0af814566 0
pn2 127.0.0.4:19888 92Da1aYg6sbenP6uwskJgEY2XWB5LwJ7bXRqc3UPeShtHWJDjDv 5d898e78244f3206e0105f421cdb071d95d111a51cd88eb5511fc0dbf4bfd95f 1
```
_Note: IPs like 127.0.0.* are not allowed actually, we are using them here for explanatory purposes only. Make sure you have real reachable remote IPs in you `popnode.conf`._

The following RPC commands are available (type `help popnode` in Console for more info):
* list-conf
* start-alias \<alias\>
* start-all
* start-missing
* start-disabled
* outputs
