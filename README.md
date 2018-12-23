# FileSystemsOS
File System using FUSE

Block Size: 512 bytes<br>
Size of inode : 128 bytes<br>
Number of inodes : 16<br>
Number of data blocks : 32<br>
Each block has 4 inodes.<br>
Each inode has 10 direct pointers<br>
There are no indirect pointers <br>
<br>
The superblock, inode and data bit maps occupy one block each.
<br><br>
The block numbers and inode numbers start from 1.
<br><br>
A directory is a file with the data block containing records (inode number, name).<br>
A directory entry with an inode number 0 indicates an invalid entry.<br>
<br>
A type member in the inode indicates whether an inode is invalid, a regular file or directory which have values 1,2,3 respectively.
<br><br>
The numRecords field in the inode  indicates the number of valid directory entries if the inode is a directory.
<br><br>
A directory can have as many entries as can be fitted in its data blocks printed to by the inodes direct pointers.
<br><br>
# Snapshots of working
![alt text](https://github.com/siri1398/FileSystemsOS/blob/master/snapshots/commands.png)
<br><br>
# Snapshots of emulated disk
![alt text](https://github.com/siri1398/FileSystemsOS/blob/master/snapshots/octal_dump1.png)
