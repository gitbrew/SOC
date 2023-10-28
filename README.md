███████╗ ██████╗  ██████╗    ██╗   ██╗██████╗ 
██╔════╝██╔═══██╗██╔════╝    ██║   ██║╚════██╗
███████╗██║   ██║██║         ██║   ██║ █████╔╝
╚════██║██║   ██║██║         ╚██╗ ██╔╝██╔═══╝ 
███████║╚██████╔╝╚██████╗     ╚████╔╝ ███████╗
╚══════╝ ╚═════╝  ╚═════╝      ╚═══╝  ╚══════╝
                                              
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


SOC is a command-line utility for Windows designed to securely delete files, including encrypted, compressed, or sparse files. It also provides the capability to clean free disk space according to the Department of Defense 5220.22-M sanitizing standard. To utilize SOC, place the file you want to delete in the same directory as the SOC executable and execute SOC from a command prompt or PowerShell.
