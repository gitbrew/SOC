 - SOC v2
Copyright (C) 2013 Kyle Barnthouse (durandal)
http://gitbrew.org A Gitbrew Release

Respectfully plaigarized from David Condrey's circa 2002 SOC program

usage:	SOC [-p passes] [-s] [-q] <file or directory>
	SOC [-p passes] -z [drive letter]

	-p [passes]	Specifies number of overwrite passes <default set to 1>
	-s		Recurse subdirectories
	-q		Do not print errors
	-z		Clean free space

Support:	Windows 9x, NT/2000, XP, and beyond


SOC is a simple Windows utility to securely delete files to include encrypted, compressed, or sparse files and clean free disk space using the Department of Defense 5220.22-M sanitizing standard.


To use SOC simply place the file you wish to delete in the same directory as the SOC executable and run SOC in a command/powershell prompt.
