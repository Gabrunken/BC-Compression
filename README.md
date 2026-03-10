DESCRIPTION



This is a very small program made to offline cook images to Block Compressed ones, useful for rendering.

It was basically made all by using LLMs, and utilizes the stb\_image and stb\_dxt libraries.



BUILD

If you are on windows, run the build\_windows.bat file.

If you are on linux/macos (unix), run the build\_unix.sh file.



USAGE

It is a command prompt program.

To use it open a terminal and run the program, including 3 main command-line-argument:

	- <file-to-compress>

	- <output-file>

	- <compression-type>

Options:
	- "-f" -> flip image vertically on load
	- "-h" -> show all commands and other information

NOTES:

Mipmap generation is yet to be done.