% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% 2015-07-03

# NAME

mergerfs - another FUSE union filesystem

# SYNOPSIS

mergerfs -o&lt;options&gt; &lt;srcpoints&gt; &lt;mountpoint&gt;

# DESCRIPTION

**mergerfs** is similar to **mhddfs**, **unionfs**, and **aufs**. Like **mhddfs** in that it too uses **FUSE**. Like **aufs** in that it provides multiple policies for how to handle behavior.

Why create **mergerfs** when those exist? **mhddfs** has not been updated in some time nor very flexible. There are also security issues when with running as root. **aufs** is more flexible than **mhddfs** but contains some hard to debug inconsistencies in behavior on account of it being a kernel driver. Neither support file attributes ([chattr](http://linux.die.net/man/1/chattr)).

# OPTIONS

###options###

* **defaults** is a shortcut for **auto_cache**. **big_writes**, **atomic_o_trunc**, **splice_read**, **splice_write**, and **splice_move** are in effect also enabled (by asking **FUSE** internally for such features) but if unavailable will be ignored. These options seem to provide the best performance.
* **minfreespace** (defaults to **4G**) is the minimum space value used for the **lfs**, **fwfs**, and **epmfs** policies. Understands 'K', 'M', and 'G' to represent kilobyte, megabyte, and gigabyte respectively.
* All FUSE functions which have a category (see below) are option keys. The syntax being **func.&lt;func&gt;=&lt;policy&gt;**. Example: **func.getattr=newest**.
* To set all function policies in a category use **category.&lt;category&gt;=&lt;policy&gt;**. Example: **category.create=mfs**.
* Options are evaluated in the order listed so if the options are **func.rmdir=rand,category.action=ff** the **action** category setting will override the **rmdir** setting.

###srcpoints###

The source points argument is a colon (':') delimited list of paths. To make it simpler to include multiple source points without having to modify your [fstab](http://linux.die.net/man/5/fstab) we also support [globbing](http://linux.die.net/man/7/glob).

```
$ mergerfs /mnt/disk\*:/mnt/cdrom /media/drives
```

The above line will use all points in /mnt prefixed with *disk* and the directory *cdrom*. The `*` (or any glob tokens) must be escaped on the command line else the shell resolve them. 

In /etc/fstab it'd look like the following:

```
# <file system>        <mount point>  <type>         <options>             <dump>  <pass>
/mnt/disk*:/mnt/cdrom  /media/drives  fuse.mergerfs  defaults,allow_other  0       0
```

**NOTE:** the globbing is done at mount or xattr update time. If a new directory is added matching the glob after the fact it will not be included.

# POLICIES

Filesystem calls are broken up into 3 categories: **action**, **create**, **search**. There are also some calls which have no policy attached due to state being kept between calls. These categories can be assigned a policy which dictates how **mergerfs** behaves. Any policy can be assigned to a category though some aren't terribly practical. For instance: **rand** (Random) may be useful for **create** but could lead to very odd behavior if used for **search**.

#### Functional  classifications ####

| Category | FUSE Functions |
|----------|----------------|
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens |
| create   | create, mkdir, mknod, symlink |
| search   | access, getattr, getxattr, ioctl, listxattr, open, readlink |
| N/A      | fallocate, fgetattr, fsync, ftruncate, ioctl, read, readdir, release, statfs, write |

**ioctl** behaves differently if its acting on a directory. It'll use the **getattr** policy to find and open the directory before issuing the **ioctl**. In other cases where something may be searched (to confirm a directory exists across all source mounts) then **getattr** will be used.

#### Policy descriptions ####

| Policy | Description |
|--------------|-------------|
| ff (first found) | Given the order of the drives act on the first one found (regardless if stat would return EACCES). |
| ffwp (first found w/ permissions) | Given the order of the drives act on the first one found which you have access (stat does not error with EACCES). |
| newest (newest file) | If multiple files exist return the one with the most recent mtime. |
| mfs (most free space) | Use the drive with the most free space available. |
| epmfs (existing path, most free space) | If the path exists on multiple drives use the one with the most free space and is greater than **minfreespace**. If no drive has at least **minfreespace** then fallback to **mfs**. |
| fwfs (first with free space) | Pick the first drive which has at least **minfreespace**. |
| lfs (least free space) | Pick the drive with least available space but more than **minfreespace**. |
| rand (random) | Pick an existing drive at random. |
| all | Applies action to all found. For searches it will behave like first found **ff**. |
| enosys, einval, enotsup, exdev, erofs | Exclusively return `-1` with `errno` set to the respective value. Useful for debugging other applications' behavior to errors. |

#### Defaults ####

| Category | Policy |
|----------|--------|
| action   | all    |
| create   | epmfs  |
| search   | ff     |

#### rename ####

[rename](http://man7.org/linux/man-pages/man2/rename.2.html) is a tricky function in a merged system. Normally if a rename can't be done atomically due to the from and to paths existing on different mount points it will return `-1` with `errno = EXDEV`. The atomic rename is most critical for replacing files in place atomically (such as securing writing to a temp file and then replacing a target). The problem is that by merging multiple paths you can have N instances of the source and destinations on different drives. Meaning that if you just renamed each source locally you could end up with the destination files not overwriten / replaced. To address this mergerfs works in the following way. If the source and destination exist in different directories it will immediately return `EXDEV`. Generally it's not expected for cross directory renames to work so it should be fine for most instances (mv,rsync,etc.). If they do belong to the same directory it then runs the `rename` policy to get the files to rename. It iterates through and renames each file while keeping track of those paths which have not been renamed. If all the renames succeed it will then `unlink` or `rmdir` the other paths to clean up any preexisting target files. This allows the new file to be found without the file itself ever disappearing. There may still be some issues with this behavior. Particularly on error. At the moment however this seems the best policy.

#### readdir ####

[readdir](http://linux.die.net/man/3/readdir) is very different from most functions in this realm. It certainly could have it's own set of policies to tweak its behavior. At this time it provides a simple **first found** merging of directories and file found. That is: only the first file or directory found for a directory is returned. Given how FUSE works though the data representing the returned entry comes from **getattr**.

It could be extended to offer the ability to see all files found. Perhaps concatenating **#** and a number to the name. But to really be useful you'd need to be able to access them which would complicate file lookup.

#### statvfs ####

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple points on the same drive will not result in double counting it's space.

**NOTE:** Since we can not (easily) replicate the atomicity of an **mkdir** or **mknod** without side effects those calls will first do a scan to see if the file exists and then attempts a create. This means there is a slight race condition. Worse case you'd end up with the directory or file on more than one mount.

# BUILDING

First get the code from [github](http://github.com/trapexit/mergerfs).

```
$ git clone https://github.com/trapexit/mergerfs.git
$ # or
$ wget https://github.com/trapexit/mergerfs/archive/master.zip
```

#### Debian / Ubuntu
```
$ sudo apt-get install g++ pkg-config git git-buildpackage pandoc debhelper libfuse-dev libattr1-dev
$ cd mergerfs
$ make deb
$ sudo dpkg -i ../mergerfs_version_arch.deb
```

#### Fedora
```
$ su -
# dnf install fuse-devel libattr-devel pandoc gcc-c++
# cd mergerfs
# make rpm
# rpm -i rpmbuild/RPMS/<arch>/mergerfs-<verion>.<arch>.rpm
```

#### Generically

Have pkg-config, pandoc, libfuse, libattr1 installed.

```
$ cd mergerfs
$ make
$ make man
$ sudo make install
```

# RUNTIME

#### .mergerfs pseudo file ####
```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mount point which allows for the runtime modification of certain **mergerfs** options. The file will not show up in **readdir** but can be **stat**'ed and manipulated via [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls.

Even if xattrs are disabled the [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls will still work.

##### Keys #####

Use `xattr -l /mount/point/.mergerfs` to see all supported keys.

##### Example #####

```
[trapexit:/tmp/mount] $ xattr -l .mergerfs
user.mergerfs.srcmounts: /tmp/a:/tmp/b
user.mergerfs.minfreespace: 4294967295
user.mergerfs.category.action: all
user.mergerfs.category.create: epmfs
user.mergerfs.category.search: ff
user.mergerfs.func.access: ff
user.mergerfs.func.chmod: all
user.mergerfs.func.chown: all
user.mergerfs.func.create: epmfs
user.mergerfs.func.getattr: ff
user.mergerfs.func.getxattr: ff
user.mergerfs.func.link: all
user.mergerfs.func.listxattr: ff
user.mergerfs.func.mkdir: epmfs
user.mergerfs.func.mknod: epmfs
user.mergerfs.func.open: ff
user.mergerfs.func.readlink: ff
user.mergerfs.func.removexattr: all
user.mergerfs.func.rename: all
user.mergerfs.func.rmdir: all
user.mergerfs.func.setxattr: all
user.mergerfs.func.symlink: epmfs
user.mergerfs.func.truncate: all
user.mergerfs.func.unlink: all
user.mergerfs.func.utimens: all

[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
ff

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.category.search ffwp .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
ffwp

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts +/tmp/c .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/a:/tmp/b:/tmp/c

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts =/tmp/c .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/c

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts '+</tmp/a:/tmp/b' .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/a:/tmp/b:/tmp/c
```

##### user.mergerfs.srcmounts #####

For **user.mergerfs.srcmounts** there are several instructions available for manipulating the list. The value provided is just as the value used at mount time. A colon (':') delimited list of full path globs.

| Instruction | Description |
|--------------|-------------|
| [list]       | set |
| +<[list]     | prepend |
| +>[list]     | append |
| -[list]      | remove all values provided |
| -<           | remove first in list |
| ->           | remove last in list |


##### misc #####

Categories and funcs take a policy as described in the previous section. When reading funcs you'll get the policy string. However, with categories you'll get a comma separated list of policies for each type found. For example: if all search functions are **ff** except for **access** which is **ffwp** the value for **user.mergerfs.category.search** will be **ff,ffwp**.

#### mergerfs file xattrs ####

While they won't show up when using [listxattr](http://linux.die.net/man/2/listxattr) **mergerfs** offers a number of special xattrs to query information about the files served. To access the values you will need to issue a [getxattr](http://linux.die.net/man/2/getxattr) for one of the following:

* **user.mergerfs.basepath:** the base mount point for the file given the current search policy
* **user.mergerfs.relpath:** the relative path of the file from the perspective of the mount point
* **user.mergerfs.fullpath:** the full path of the original file given the search policy
* **user.mergerfs.allpaths:** a NUL ('\0') separated list of full paths to all files found

```
[trapexit:/tmp/mount] $ ls
A B C
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.fullpath A
/mnt/a/full/path/to/A
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.basepath A
/mnt/a
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.relpath A
/full/path/to/A
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.allpaths A | tr '\0' '\n'
/mnt/a/full/path/to/A
/mnt/b/full/path/to/A
```

# TIPS / NOTES

* If you don't see some directories / files you expect in a merged point be sure the user has permission to all the underlying directories. If `/drive0/a` has is owned by `root:root` with ACLs set to `0700` and `/drive1/a` is `root:root` and `0755` you'll see only `/drive1/a`.
* Since POSIX gives you only error or success on calls its difficult to determine the proper behavior when applying the behavior to multiple targets. Generally if something succeeds when reading it returns the data it can. If something fails when making an action we continue on and return the last error.
* The recommended options are **defaults,allow_other**. The **allow_other** is to allow users who are not the one which executed mergerfs access to the mountpoint. **defaults** is described above and should offer the best performance. It's possible that if you're running on an older platform the **splice** features aren't available and could error. In that case simply use the other options manually.
* Remember that some policies mixed with some functions may result in strange behaviors. Not that some of these behaviors and race conditions couldn't happen outside **mergerfs** but that they are far more likely to occur on account of attempt to merge together multiple sources of data which could be out of sync due to the different policies.
* An example: [Kodi](http://kodi.tv) can apparently use directory [mtime](http://linux.die.net/man/2/stat) to more efficiently determine whether or not to scan for new content rather than simply performing a full scan. If using the current default **getattr** policy of **ff** its possible **Kodi** will miss an update on account of it returning the first directory found's **stat** info and its a later directory on another mount which had the **mtime** recently updated. To fix this you will want to set **func.getattr=newest**. Remember though that this is just **stat**. If the file is later **open**'ed or **unlink**'ed and the policy is different for those then a completely different file or directory could be acted on.
* Due to previously mentioned issues its generally best to set **category** wide policies rather than individual **func**'s. This will help limit the confusion of tools such as [rsync](http://linux.die.net/man/1/rsync).

# Known Issues / Bugs

* Performing an [initgroups](http://linux.die.net/man/3/initgroups) or [setgroups](http://linux.die.net/man/2/setgroups) for every call (see below) would be relatively expensive and currently not [done](https://github.com/trapexit/mergerfs/issues/88). Some caching system will need to be created to limit calls to [getgroups](http://linux.die.net/man/2/getgroups) and [getpwuid_r](http://linux.die.net/man/3/getpwuid_r). Until that's done if a file / directory set to be accessable only by a group may not work if the primary group of the user making the call is not used. Limiting the switching of IDs is also in the works which will help as well.

# FAQ

*It's mentioned that there are some security issues with mhddfs. What are they? How does mergerfs address them?*

[mhddfs](https://github.com/trapexit/mhddfs) tries to handle being run as **root** by calling [getuid()](https://github.com/trapexit/mhddfs/blob/cae96e6251dd91e2bdc24800b4a18a74044f6672/src/main.c#L319) and if it returns **0** then it will [chown](http://linux.die.net/man/1/chown) the file. Not only is that a race condition but it doesn't handle many other situations. Rather than attempting to simulate POSIX ACL behaviors the proper behavior is to use [seteuid](http://linux.die.net/man/2/seteuid) and [setegid](http://linux.die.net/man/2/setegid), become the user making the original call and perform the action as them. This is how [mergerfs](https://github.com/trapexit/mergerfs) handles things.  

If you are familiar with POSIX standards you'll know that this behavior poses a problem. **seteuid** and **setegid** affect the whole process and **libfuse** is multithreaded by default. We'd need to lock access to **seteuid** and **setegid** with a mutex so that the several threads aren't stepping on one another and files end up with weird permissions and ownership. This however wouldn't scale well. With lots of calls the contention on that mutex would be extremely high. Thankfully on Linux and OSX we have a better solution.  

OSX has a [non-portable pthread extension](https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/pthread_setugid_np.2.html) for per-thread user and group impersonation. When building on OSX mergerfs will use this without any mutexes.  

Linux does not support [pthread_setugid_np](https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/pthread_setugid_np.2.html) but user and group IDs are a per-thread attribute though documentation on that fact or how to manipulate them is not well distributed. From the **4.00** release of the Linux man-pages project for [setuid](http://man7.org/linux/man-pages/man2/setuid.2.html)  

> At the kernel level, user IDs and group IDs are a per-thread attribute.  However, POSIX requires that all threads in a process share the same credentials.  The NPTL threading implementation handles the POSIX requirements by providing wrapper functions for the various system calls that change process UIDs and GIDs.  These wrapper functions (including the one for setuid()) employ a signal-based technique to ensure that when one thread changes credentials, all of the other threads in the process also change their credentials.  For details, see nptl(7).  

Turns out the setreuid syscalls apply only to the thread. GLIBC hides this away using RT signals and other tricks. Taking after **Samba** mergerfs uses **syscall(SYS_setreuid,...)** to set the callers credentials for that thread only. Jumping back to **root** as necessary should escalated privileges be needed (for instance: to clone paths).  
