/*
 * Copyright 2008-2016 Douglas Patriarche
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>      // isalnum
#include <fcntl.h>
#include <grp.h>        // for getgrgid(3)
#include <pthread.h>
#include <pwd.h>        // for getpwuid(3)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>   // for S_IS*(3)
#include <unistd.h>

#include <iostream>
#include <list>
#include <sstream>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "fsevents.h"
#include "MutexLocker.h"
#include "XmlStrBuilder.h"

//-----------------------------------------------------------------------------

enum EventType_t
{
    NONE,
    ADD,
    DELETE,
    CHANGE
};

struct Event_t
{
    EventType_t type_m;
    char * path_m;
    bool printRequired_m;

    Event_t()
        : type_m(NONE),
          path_m(NULL),
          printRequired_m(false)
    {}
};

//-----------------------------------------------------------------------------

static bool isDebug_s = false;
static bool isOutputInXml_s = false;
static int64_t eventCounter_s = 0;
static pthread_mutex_t mutex_s = PTHREAD_MUTEX_INITIALIZER;

typedef std::set<std::string> PathSet_t;
static PathSet_t monPathSet_s; // Protected by mutex_s

typedef std::vector<std::string> PathVec_t;
static PathVec_t monPathVec_s; // Protected by mutex_s

//-----------------------------------------------------------------------------
// Terminate the process with an optional error message.

static void terminate()
{
    perror(NULL);
    exit(1);
}

//-----------------------------------------------------------------------------
// Replace all substrings in a string with a new substring.

static std::string strReplaceAll(std::string const & in, std::string const & oldSubStr, std::string const & newSubStr)
{
    size_t oldSubStrSize = oldSubStr.length();

    std::string s(in);
    std::stack<size_t> positions;

    for (size_t pos = s.find(oldSubStr); pos != std::string::npos; pos = s.find(oldSubStr, pos + oldSubStr.length())) {
        positions.push(pos);
    }

    while (!positions.empty()) {
        size_t pos = positions.top();
        positions.pop();
        s.replace(pos, oldSubStrSize, newSubStr);
    }

    return s;
}

//-----------------------------------------------------------------------------
// Add escapes to a string to make it safe to be included as content in XML.

static std::string strMakeXmlSafe(std::string const & str)
{
    std::string s = strReplaceAll(str, "&", "&amp;");
    s = strReplaceAll(s, "<", "&lt;");
    s = strReplaceAll(s, ">", "&gt;");
    return s;
}

//-----------------------------------------------------------------------------
// Is a specified file system path under on eof the monitored paths?

static bool isMonitoredPath(std::string const & testPath)
{
    if (isDebug_s) {
        printf("DBG: isMonitoredPath( %s )\n", testPath.c_str());
    }

    for (PathVec_t::const_iterator iter = monPathVec_s.begin(); iter != monPathVec_s.end(); ++iter) {
        std::string const & monPath = *iter;
        if (monPath.size() <= testPath.size()) {
            if (testPath.compare(0, monPath.size(), monPath) == 0) {
                // The monPath matches the prefix of the testPath.  There are now two possibilities: (1) the monPath is a file, in which  case the match must be exact; or (2) the monPath is for a directory, in which case the match must either be exact, or the next char in the testPath must be a slash.
                if (monPath.size() == testPath.size()) {
                    if (isDebug_s) {
                        printf("DBG:   Matched exact: %s\n", monPath.c_str());
                    }
                    return true;
                }
                else if (testPath[monPath.size()] == '/') {
                    if (isDebug_s) {
                        printf("DBG:   Matched parent dir: %s\n", monPath.c_str());
                    }
                    return true;
                }
            }
        }

        if (isDebug_s) {
            printf("DBG:   No match against %s\n", monPath.c_str());
        }
    }

    if (isDebug_s) {
        printf("DBG:   No match against %ld monitored paths\n", monPathVec_s.size());
    }

    return false;
}

//-----------------------------------------------------------------------------
// Convert a mode number to an ls-style mode string.

static void getModeString(int32_t mode, char * buf)
{
    buf[10] = '\0';
    buf[9] = mode & 0x01 ? 'x' : '-';
    buf[8] = mode & 0x02 ? 'w' : '-';
    buf[7] = mode & 0x04 ? 'r' : '-';
    buf[6] = mode & 0x08 ? 'x' : '-';
    buf[5] = mode & 0x10 ? 'w' : '-';
    buf[4] = mode & 0x20 ? 'r' : '-';
    buf[3] = mode & 0x40 ? 'x' : '-';
    buf[2] = mode & 0x80 ? 'w' : '-';
    buf[1] = mode & 0x100 ? 'r' : '-';
    if (S_ISFIFO(mode)) {
        buf[0] = 'p';
    }
    else if (S_ISCHR(mode)) {
        buf[0] = 'c';
    }
    else if (S_ISDIR(mode)) {
        buf[0] = 'd';
    }
    else if (S_ISBLK(mode)) {
        buf[0] = 'b';
    }
    else if (S_ISLNK(mode)) {
        buf[0] = 'l';
    }
    else if (S_ISSOCK(mode)) {
        buf[0] = 's';
    }
    else {
        buf[0] = '-';
    }
}

//-----------------------------------------------------------------------------
// Return a string representation of a node type.

static char const * getVnodeTypeString(int32_t mode)
{
    char const * str_to_ret = 0;
    if (S_ISFIFO(mode)) {
        str_to_ret = "VFIFO";
    }
    else if (S_ISCHR(mode)) {
        str_to_ret = "VCHR";
    }
    else if (S_ISDIR(mode)) {
        str_to_ret = "VDIR";
    }
    else if (S_ISBLK(mode)) {
        str_to_ret = "VBLK";
    }
    else if (S_ISLNK(mode)) {
        str_to_ret = "VLNK";
    }
    else if (S_ISSOCK(mode)) {
        str_to_ret = "VSOCK";
    }
    else {
        str_to_ret = "VREG";
    }
    return str_to_ret;
}

//-----------------------------------------------------------------------------
// Get the process name for a PID.

static std::string getProcessName(pid_t pid)
{
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) != -1) {
        return std::string(kp.kp_proc.p_comm);
    }
    return std::string("???");
}

//-----------------------------------------------------------------------------
// Get the group name for a GID.

std::string getGroupName(gid_t gid)
{
    struct group * grp = getgrgid(gid);
    if (grp == NULL) {
        return std::string();
    }
    return std::string(grp->gr_name);
}

//-----------------------------------------------------------------------------
// Get the user name for a UID.

std::string getUserName(uid_t uid)
{
    struct passwd * pwd = getpwuid(uid);
    if (pwd == NULL) {
        return std::string();
    }
    return std::string(pwd->pw_name);
}

//-----------------------------------------------------------------------------
// Print this programs help info.

static void printUsage()
{
    fprintf(stderr, "\n");
    fprintf(stderr, "filemon\n");
    fprintf(stderr, "Copyright 2008-2016 Douglas Patriarche\n\n");
    fprintf(stderr,
            "filemon comes with ABSOLUTELY NO WARRANTY. This is free software, and\n"
            "you are welcome to redistribute it under certain conditions. See:\n"
            "    http://www.gnu.org/licenses/quick-guide-gplv3.html\n"
            "for further details.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: filemon [-dhx] [dirpath ...]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  -d :   print debug info\n");
    fprintf(stderr, "  -h :   print help\n");
    fprintf(stderr, "  -x :   print output in XML form\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Zero or more directory paths can be specified to be monitored.\n");
    fprintf(stderr, "Once the program is running, additional commands can be input\n");
    fprintf(stderr, "through stdin.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Interactive stdin commands:\n");
    fprintf(stderr, "  add:<path>  - Add a monitored path\n");
    fprintf(stderr, "  del:<path>  - Delete a monitored path\n");
    fprintf(stderr, "  clr         - Clear all monitored paths\n");
    fprintf(stderr, "  die         - Terminate the program\n");
}

//-----------------------------------------------------------------------------
// Process the program's input options, setting static option flags. Returns the index of the first non-option argument, or -1 if there are no additional non-option arguments.

extern int optind;

static int processOptions(int argc, char *argv[])
{
    bool isError = false;

    char c;
    while ((c = getopt(argc, argv, "dhx")) != -1) {
        switch (c) {
            case 'd':
                isDebug_s = true;
                break;
            case 'h':
                printUsage();
                exit(0);
                break;
            case 'x':
                isOutputInXml_s = true;
                break;
            case '?':
                fprintf(stderr, "Unrecognized option: -%c\n", optopt);
                isError = true;
                break;
        }
    }

    if (isError) {
        printUsage();
        exit(1);
    }

    // Return the index of the first
    return optind < argc ? optind : -1;
}

//-----------------------------------------------------------------------------
// Remove any number of a specific trailing character from an input string.

static void eraseTrailingChar(char * s, char c)
{
    for (size_t len = strlen(s); s[len - 1] == c; --len) {
        s[len - 1] = '\0';
    }
}

//-----------------------------------------------------------------------------
// Process an input command string.

static void processInputCmd(char * line)
{
    MUTEX_LOCK_UNTIL_SCOPE_EXIT(&mutex_s);

    if (isDebug_s) {
        printf("DBG: processInputCmd: %s\n", line);
    }

    // Update the monitor path set.
    if (strncmp(line, "add:", 4) == 0) {
        char * path = line + 4;
        eraseTrailingChar(path, '/');
        monPathSet_s.insert(std::string(path));
    }
    else if (strncmp(line, "del:", 4) == 0) {
        char * path = line + 4;
        eraseTrailingChar(path, '/');
        monPathSet_s.erase(std::string(path));
    }
    else if (strcmp(line, "clr") == 0) {
        monPathSet_s.clear();
    }
    else if (strcmp(line, "die") == 0) {
        if (isDebug_s) {
            printf("DBG: Terminating\n");
        }
        exit(0);
    }

    // Regenerate the monitored path vector using the new monitored path set.
    monPathVec_s.clear();
    for (PathSet_t::iterator iter = monPathSet_s.begin(); iter != monPathSet_s.end(); ++iter) {
        monPathVec_s.push_back(*iter);
    }

    if (isDebug_s) {
        printf("DBG: MONITORED PATH SET:\n");
        for (PathSet_t::iterator iter = monPathSet_s.begin(); iter != monPathSet_s.end(); ++iter) {
            printf("DBG:   - %s\n", iter->c_str());
        }
        printf("DBG: MONITORED PATH VECTOR:\n");
        for (PathVec_t::iterator iter = monPathVec_s.begin(); iter != monPathVec_s.end(); ++iter) {
            printf("DBG:   - %s\n", iter->c_str());
        }
    }

    if (isDebug_s) {
        printf("DBG: processInputCmd: DONE\n");
    }
}

//-----------------------------------------------------------------------------
// Process a FS event and output information about it in the terse format.

static void processEventTerse(char * buf, size_t size)
{
    MUTEX_LOCK_UNTIL_SCOPE_EXIT(&mutex_s);

    /* Event structure in memory:
     *
     *   event type: 4 bytes
     *   event pid:  sizeof(pid_t) (4 on darwin) bytes
     *   arg:
     *     argtype:  2 bytes
     *     arglen:   2 bytes
     *     argdata:  arglen bytes
     *   arg:
     *     ...
     *   lastarg:
     *     argtype:  2 bytes = 0xb33f
     */
    int pos = 0;
    while (pos < size) {
        eventCounter_s++;

        enum { MAX_NUM_EVENTS = 2 };
        Event_t events[MAX_NUM_EVENTS];
        int eventIndex = 0;

        int32_t eventType = *((int32_t *)((char *)buf + pos));
        pos += 4;

        switch (eventType) {
            case FSE_CREATE_FILE:
            case FSE_CREATE_DIR:
                events[0].type_m = ADD;
                break;
            case FSE_DELETE:
                events[0].type_m = DELETE;
                break;
            case FSE_STAT_CHANGED:
            case FSE_FINDER_INFO_CHANGED:
            case FSE_CHOWN:
                events[0].type_m = CHANGE;
                break;
            case FSE_EXCHANGE:
                events[0].type_m = CHANGE;
                events[1].type_m = CHANGE;
                break;
            case FSE_RENAME:
                events[0].type_m = DELETE;
                events[1].type_m = ADD;
                break;
            case FSE_INVALID:
            default:
                break;
        }

        pid_t pid = *((pid_t *) (buf + pos));
        pos += sizeof(pid_t);
        
        std::string processName = getProcessName(pid);

        while (true) {
            u_int16_t argtype = *((u_int16_t *)(buf + pos));
            pos += 2;

            if (argtype == FSE_ARG_DONE) {
                break;
            }

            u_int16_t arglen = *((u_int16_t *)(buf + pos));
            pos += 2;

            switch (argtype) {
                case FSE_ARG_VNODE:
                case FSE_ARG_STRING:
                case FSE_ARG_PATH:
                    if (eventIndex < MAX_NUM_EVENTS) {
                        events[eventIndex].path_m = buf + pos;
                        events[eventIndex].printRequired_m = isMonitoredPath(std::string(buf + pos));
                        eventIndex += 1;
                    }
                    break;
                default:
                    break;
            }

            pos += arglen;
        }

        for (int i = 0; i < MAX_NUM_EVENTS; ++i) {
            if (events[i].printRequired_m) {
                switch (events[i].type_m) {
                    case ADD:
                        printf("ADD:%s - pid %d (%s)\n", events[i].path_m, pid, processName.c_str());
                        break;
                    case DELETE:
                        printf("DEL:%s - pid %d (%s)\n", events[i].path_m, pid, processName.c_str());
                        break;
                    case CHANGE:
                        printf("CHG:%s - pid %d (%s)\n", events[i].path_m, pid, processName.c_str());
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
// Process a FS event and output information about it in the XML format.

static void processEventAsXml(char * buf, size_t size)
{
    MUTEX_LOCK_UNTIL_SCOPE_EXIT(&mutex_s);

    /* Event structure in memory:
     *
     *   event type: 4 bytes
     *   event pid:  sizeof(pid_t) (4 on darwin) bytes
     *   arg:
     *     argtype:  2 bytes
     *     arglen:   2 bytes
     *     argdata:  arglen bytes
     *   arg:
     *     ...
     *   lastarg:
     *     argtype:  2 bytes = 0xb33f
     */
    int pos = 0;

    XmlStrBuilder_t xml;

    while (pos < size) {
        eventCounter_s++;

        bool shouldPrint = false;
        xml.clear();

        int32_t eventType = *((int32_t *)((char *)buf + pos));
        pos += 4;

        switch (eventType) {
            case FSE_CREATE_FILE:
                xml.pushTag("create-file");
                break;
            case FSE_DELETE:
                xml.pushTag("delete");
                break;
            case FSE_STAT_CHANGED:
                xml.pushTag("stat-changed");
                break;
            case FSE_RENAME:
                xml.pushTag("rename");
                break;
            case FSE_CONTENT_MODIFIED:
                xml.pushTag("content-modified");
                break;
            case FSE_EXCHANGE:
                xml.pushTag("exchange");
                break;
            case FSE_FINDER_INFO_CHANGED:
                xml.pushTag("finder-info-changed");
                break;
            case FSE_CREATE_DIR:
                xml.pushTag("create-dir");
                break;
            case FSE_CHOWN:
                xml.pushTag("chown");
                break;
            case FSE_INVALID:
            default:
                xml.pushTag("invalid");
                break;
        }

        xml.addTagAndVararg("eventNumber", "%lld", eventCounter_s);

        pid_t pid = *((pid_t *) (buf + pos));
        pos += sizeof(pid_t);

        xml.pushTag("process");
        xml.addTagAndVararg("id", "%d", pid);
        xml.addTagAndValue("name", strMakeXmlSafe(getProcessName(pid)));
        xml.popTag();

        while (true) {
            u_int16_t argtype = *((u_int16_t *)(buf + pos));
            pos += 2;

            if (argtype == FSE_ARG_DONE) {
                xml.addTagAndVararg("done", "0x%x", argtype);
                break;
            }

            u_int16_t arglen = *((u_int16_t *)(buf + pos));
            pos += 2;

            switch (argtype) {
                case FSE_ARG_VNODE: {
                    std::string path(buf + pos);
                    shouldPrint = shouldPrint || isMonitoredPath(path);

                    xml.addTagAndValue("vnode", strMakeXmlSafe(path));
                    break;
                }
                case FSE_ARG_STRING: {
                    std::string path(buf + pos);
                    shouldPrint = shouldPrint || isMonitoredPath(path);

                    xml.addTagAndValue("string", strMakeXmlSafe(path));
                    break;
                }
                case FSE_ARG_PATH: { // not in kernel
                    std::string path(buf + pos);
                    shouldPrint = shouldPrint || isMonitoredPath(path);

                    xml.addTagAndValue("path", strMakeXmlSafe(path));
                    break;
                }
                case FSE_ARG_INT32: {
                    int32_t value = *((int32_t *)(buf + pos));
                    xml.addTagAndVararg("int32", "%d", value);
                    break;
                }
                case FSE_ARG_INT64: { // not supported in kernel yet
                    int64_t value = *((int64_t *)(buf + pos));
                    xml.addTagAndVararg("int64", "%lld", value);
                    break;
                }
                case FSE_ARG_RAW: {
                    xml.pushTag("raw");
                    xml.addTagAndVararg("length", "%d", arglen);
                    xml.popTag();
                    break;
                }
                case FSE_ARG_INO: {
                    ino_t value = *((ino_t *)(buf + pos));
                    xml.addTagAndVararg("inode", "%d", value);
                    break;
                }
                case FSE_ARG_UID: {
                    uid_t uid = *((uid_t *)(buf + pos));

                    xml.pushTag("uid");
                    xml.addTagAndVararg("int", "%d", uid);
                    xml.addTagAndValue("name", strMakeXmlSafe(getUserName(uid)));
                    xml.popTag();
                    break;
                }
                case FSE_ARG_DEV: {
                    dev_t device = *((dev_t *) (buf + pos));

                    xml.pushTag("device");
                    xml.addTagAndVararg("value", "0x%08x", device);
                    xml.addTagAndVararg("major", "%d", (device >> 24) & 0xff);
                    xml.addTagAndVararg("minor", "%d", device & 0xffffff);
                    xml.popTag();
                    break;
                }
                case FSE_ARG_MODE: {
                    int32_t mode = *((int32_t *)(buf + pos));
                    char modeStr [16];
                    getModeString(mode, modeStr);
                    char const * vnodeType = getVnodeTypeString(mode);

                    xml.pushTag("mode");
                    xml.addTagAndVararg("int", "0x%x", mode);
                    xml.addTagAndVararg("vnode-type", "%s", vnodeType);
                    xml.addTagAndVararg("str", "%s", modeStr);
                    xml.popTag();
                    break;
                }
                case FSE_ARG_GID: {
                    gid_t gid = *((gid_t *)(buf + pos));

                    xml.pushTag("gid");
                    xml.addTagAndVararg("int", "%d", gid);
                    xml.addTagAndValue("name", strMakeXmlSafe(getGroupName(gid)));
                    xml.popTag();
                    break;
                }
                default: {
                    xml.addTagAndVararg("unknown-arg", "%d", arglen);
                    break;
                }
            }
            pos += arglen;
        }

        xml.popTag();

        if (shouldPrint) {
            printf("%s", xml.str().c_str());
        }
    }
}

//-----------------------------------------------------------------------------
// The pthread worker entry function.

static void * workerThreadEntry(void * arg)
{
    char buf [8192];

    // Build the list of event types, specifying whether we care about them or not.
    int8_t eventList [FSE_MAX_EVENTS];
    eventList[FSE_CREATE_FILE]         = FSE_REPORT;
    eventList[FSE_DELETE]              = FSE_REPORT;
    eventList[FSE_STAT_CHANGED]        = FSE_REPORT;
    eventList[FSE_RENAME]              = FSE_REPORT;
    eventList[FSE_CONTENT_MODIFIED]    = FSE_REPORT;
    eventList[FSE_EXCHANGE]            = FSE_REPORT;
    eventList[FSE_FINDER_INFO_CHANGED] = FSE_REPORT;
    eventList[FSE_CREATE_DIR]          = FSE_REPORT;
    eventList[FSE_CHOWN]               = FSE_REPORT;
    eventList[FSE_XATTR_MODIFIED]      = FSE_IGNORE;
    eventList[FSE_XATTR_REMOVED]       = FSE_IGNORE;

    // Open the fsevents device to a temporary FD.  This will be used to talk to the device so we can clone the FD while configuring event monitoring parameters.
    int tempfd = open("/dev/fsevents", 0, O_RDONLY);
    if (tempfd < 0) {
        terminate();
    }

    // Tell the fsevents device the desired event monitoring parameters like the event types that we care about and the event queue depth to maintain. This returns the real FD to use.
    int fd;
    fsevent_clone_args fseventsCloneArgs;
    fseventsCloneArgs.event_list = eventList;
    fseventsCloneArgs.num_events = sizeof(eventList);
    fseventsCloneArgs.event_queue_depth = 0x1000;
    fseventsCloneArgs.fd = &fd;

    if (ioctl(tempfd, FSEVENTS_CLONE, &fseventsCloneArgs) < 0) {
        terminate();
    }

    // Now that we have the real FD we can close the temp FD.
    close(tempfd);

    // Print to stderr that we started. This MUST print to stderr because that the is the stream on which the program that exec'ed this thread will be listening.
    fprintf(stderr, "STARTED\n");

    // Spin on the FD reading event data. Note that we must read at least 2048 bytes at a time on this fd, to get data. Also we must read quickly! Newer events can be lost in the internal kernel event buffer if we take too long on an earlier. To this end the bigger the buffer the better:fewer calls to read().
    size_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        if (isOutputInXml_s) {
            processEventAsXml(buf, n);
        }
        else {
            processEventTerse(buf, n);
        }
    }

    return NULL;
}

//-----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    // Set line buffering for stdout.
    setvbuf(stdout, NULL, _IOLBF, 0);

    // Handle command line options.
    int argIndex = processOptions(argc, argv);

    // Add all paths that were provided as command line arguments.
    if (argIndex != -1) {
        for (; argIndex < argc; ++argIndex) {
            char * path = argv[argIndex];
            char cmdBuf[4 + strlen(path) + 1];
            snprintf(cmdBuf, sizeof(cmdBuf), "add:%s", path);
            processInputCmd(cmdBuf);
        }
    }

    // Check that we have the proper permissions to run.
    uid_t uid = getuid();
    uid_t euid = geteuid();
    std::string uname = getUserName(uid);
    std::string euname = getUserName(euid);
    if (euid != 0) {
        fprintf(stderr, "Error: filemon must run with root permissions\n"
                "uid = %d (%s), effective uid = %d (%s)\n", uid, uname.c_str(), euid, euname.c_str());
        return -1;
    }

    if (isDebug_s) {
        printf("DBG: uid = %d (%s), effective uid = %d (%s)\n", uid, uname.c_str(), euid, euname.c_str());
    }

    // Create a worker thread to handle the processing of fsevents info.
    pthread_t worker;
    if (pthread_create(&worker, NULL, workerThreadEntry, NULL) != 0) {
        terminate();
    }

    // Spin on stdin reading commands.
    char buf [128 + PATH_MAX]; // Room for cmd + path
    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        // Eliminate trailing newlines.
        eraseTrailingChar(buf, '\n');
        processInputCmd(buf);
    }

    return 0;
}
