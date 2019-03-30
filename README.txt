Compile: type make in this directory

Run: Server and client filebases are determined by the directory where the application was run, as such, recommendations are:
	Client: cd ClientFiles
		../client <ip address> <port number> <secret key>

	Server:	cd ServerFiles
		../server <port number> <secret key>

Port numbers must match to even start into the process.
If secret keys do not match, server continues to run, but client recieves an error message from server and then terminates
If keys match user is prompted in client to continue issuing commands, server will display any time client makes a request to the server 
	including request number from current connect,request type in name form and success status

Client is given the option between 4 overarching commands

cp: copy
4 variations depending on location of files in command
prefacing filename with c: specifies file as in cloud

cp file1 file2
	local copy, makes a copy of file1 and names new file, file2, no request to server
cp file1 c:file2
	store file1 on server directory with the name file2
cp c:file1 file2
	retrive file from server named file1 and save to client directory as file2
cp c:file1 c:file2
	cloud copy, makes a copy of file1 in server directory and names new file file2, also on the server
	
rm: remove
2 variations, either removing file locally or on the cloud
prefacing filename with c: specifies file as in cloud

rm file1
	deletes local file named file1, no request to server
rm c:file1
	deletes cloud file named file1

list:
typing list will retrieve the name of all the files in the server's directory

quit:
type quit to quit
also triggers on ctrl-d or EOF

Limitations:
Binary buffer is 100k bytes, this is in accordance with spec, but files above this size will be truncated
list structure predefined as holding 40 filenames that are at most 40 characters
	40 char limit was spec, but max number of files in folder chosen as 40, so if there are more than 40 files in the folder, not all will be displayed
Running either program validates number of arguements but does not ensure inputs are sane
	if arguments intentionally deny expectations, proper running not assured.
Remainder of known limitation are defined as assumptions in spec.
