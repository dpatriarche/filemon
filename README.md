# FileMonitor

A command line file system monitor for the Mac.

This utility is useful for finding what processes are making changes on your file system, or for findout out what changes a specific process are making. This program uses Darwin's low-level fsevents API, that same API used by Time Machine to build its log of changed files for the next incremental backup. This API requires root access, so the program must be run as root.

## Features

- Monitor any number of file system paths for changes, using Darwin's low-level fsevents API.
- Low overhead for efficiently monitoring high volumes of file system events.
- Add and remove monitored paths on the fly.
- Terse one line event notification (add, change, or delete of a path with the PID and process name) or Verbose XML event notification.

## Requirements

- Runtime: macOS 10.12 or later
- Build: Xcode 8 and 10.12 SDK or later
- Root access to monitored system (program must run as root)

Note that there is no reason why this project couldn't be compiled and run on much earlier versions of macOS, I just don't have anything earlier than 10.12 to test on. In the misty past I originally wrote filemon to run on macOS 10.6, and nothing has changed since that should have invalidated that.

## Usage

```
Usage: filemon [-dhx] [dirpath ...]

  -d :   print debug info
  -h :   print help
  -x :   print output in XML form

Zero or more directory paths can be specified to be monitored.
Once the program is running, additional commands can be input
through stdin.

Interactive stdin commands:
  add:<path>  - Add a monitored path
  del:<path>  - Delete a monitored path
  clr         - Clear all monitored paths
  die         - Terminate the program
```

## Examples

Watch user alice's home directory for changes:

```
$ sudo ./filemon /Users/name
STARTED
CHG:/Users/alice/Library/Preferences/com.apple.AddressBook.plist.TZwyEjg - pid 303 (cfprefsd)
CHG:/Users/alice/Library/Preferences/com.apple.AddressBook.plist.TZwyEjg - pid 303 (cfprefsd)
DEL:/Users/alice/Library/Preferences/com.apple.AddressBook.plist.TZwyEjg - pid 303 (cfprefsd)
ADD:/Users/alice/Library/Preferences/com.apple.AddressBook.plist - pid 303 (cfprefsd)
ADD:/Users/alice/Library/Containers/com.tapbots.TweetbotMac/Data/Library/Application Support/Tweetbot/16741670.accountd/account - pid 4465 (Tweetbot)
CHG:/Users/alice/Library/Containers/com.tapbots.TweetbotMac/Data/Library/Application Support/Tweetbot/16741670.accountd/account - pid 4465 (Tweetbot)
CHG:/Users/alice/Library/Containers/com.tapbots.TweetbotMac/Data/Library/Application Support/Tweetbot/16741670.accountd/account - pid 895 (mdflagwriter)
ADD:/Users/alice/Library/Containers/com.tapbots.TweetbotMac/Data/Library/Application Support/Tweetbot/474075044.accountd/account - pid 4465 (Tweetbot)
CHG:/Users/alice/Library/Containers/com.tapbots.TweetbotMac/Data/Library/Application Support/Tweetbot/474075044.accountd/account - pid 4465 (Tweetbot)
CHG:/Users/alice/Library/Containers/com.tapbots.TweetbotMac/Data/Library/Application Support/Tweetbot/474075044.accountd/account - pid 895 (mdflagwriter)
CHG:/Users/alice/Library/Saved Application State/com.googlecode.iterm2.savedState/data.data - pid 312 (iTerm2)
CHG:/Users/alice/Library/Saved Application State/com.googlecode.iterm2.savedState/windows.plist - pid 312 (iTerm2)
CHG:/Users/alice/Library/Saved Application State/com.googlecode.iterm2.savedState/window_2.data - pid 312 (iTerm2)
```

Watch user alice's Library folder for changes made by the 'cfprefsd' process:

```
$ sudo ./filemon /Users/doug/Library | grep cfprefsd
STARTED
ADD:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.xzapUfB - pid 303 (cfprefsd)
CHG:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.xzapUfB - pid 303 (cfprefsd)
CHG:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.xzapUfB - pid 303 (cfprefsd)
CHG:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.xzapUfB - pid 303 (cfprefsd)
CHG:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.xzapUfB - pid 303 (cfprefsd)
DEL:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.xzapUfB - pid 303 (cfprefsd)
ADD:/Users/doug/Library/Preferences/com.apple.AddressBook.plist - pid 303 (cfprefsd)
ADD:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.oUaO4p8 - pid 303 (cfprefsd)
CHG:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.oUaO4p8 - pid 303 (cfprefsd)
CHG:/Users/doug/Library/Preferences/com.apple.AddressBook.plist.oUaO4p8 - pid 303 (cfprefsd)
```
