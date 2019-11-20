/*
BlacklightStringify
5/24/19 15:10
*/

/*
BlacklightStringify is an application that converts a file into a buffer usable by a C++ program
*/

// STL includes
#include <fstream>
#include <iomanip>
#include <iostream>
#include <streambuf>
#include <string>
#include <string_view>
#include <vector>

int main(int argc, const char* argv[])
{
	// ensure we have args
	if (argc < 2)
	{
		std::cout << "Usage: " << argv[0] << " <path:string> [args]\nFor a list of arguments, use -h\n";
		return 1;
	}
	// variable declarations
	std::string guard = "CHANGEME";
	std::string fileName;
	std::string outFileName;
	std::string varName = "buf";
	bool legacy = false;
	bool parsedName = false;
	// parse args
	for (int i = 1; i < argc; ++i)
	{
		std::string_view arg(argv[i]);
		if (arg == "-h" || arg == "--help")
		{
			std::cout << "Usage: " << argv[0] << " <path:string> [args]\nArgs:\n\t-h\tDisplay this help message\n\t-l\tOuutput C-style legacy buffer\n\t-o\tSpecify an output file\n\t-n\tSpecify the name of the buffer\n\t-g\tSpecify the name for the header guard (default: CHANGEME)\n";
			return 1;
		}
		else if (arg == "-l")
		{
			legacy = true;
		}
		else if (arg == "-o")
		{
			outFileName = argv[++i];
		}
		else if (arg == "-n")
		{
			varName = argv[++i];
		}
		else if (arg == "-g")
		{
			guard = argv[++i];
		}
		else
		{
			if (parsedName == true)
			{
				std::cout << "Did not expect a second file input, but got " << argv[i] << '\n';
				return 1;
			}
			else
			{
				fileName = argv[i];
				parsedName = true;
			}
		}
	}
	// check to make sure we got a fileName
	if (fileName.size() == 0)
	{
		std::cout << "Did not receive a file path. For usage, use -h\n";
		return 1;
	}
	std::ifstream file(fileName, std::ios_base::binary);
	// make sure we have a handle to the file
	if (file.good() == false)
	{
		std::cout << "Unable to open file " << fileName << '\n';
		return 1;
	}
	// read the file into a vector
	std::vector<uint8_t> fileData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	// open an output stream to the output, if specified
	std::ofstream outFile;
	if (outFileName.size() != 0)
	{
		outFile.open(outFileName);
	}
	// create a generic stream ref to stdout or the output file, if desired
	std::ostream& out = (outFileName.size() != 0) ? outFile : std::cout;
	// header guards
	out << "#ifndef " << guard << "_H_\n#define " << guard << "_H_\n\n";
	// output the include for array if we are not using legacy, as well as the variable declaration
	if (legacy == false)
	{
		out << "#include <array>\n#include <cstdint>\n\n";
		out << "std::array<uint8_t, " << fileData.size() << "> " << varName;
	}
	else
	{
		out << "#include <stdint.h>\n\n";
		out << "const uint8_t " << varName << '[' << fileData.size() << ']';
	}
	// variable definition
	out << " = { ";
	// set output to hex
	out << std::hex;
	// output our data
	for (size_t i = 0; i < fileData.size(); ++i)
	{
		out << "0x" << static_cast<unsigned>(fileData[i]);
		// print comma and space
		if (i != (fileData.size() - 1))
		{
			out << ", ";
		}
	}
	// finish definition
	out << " };\n\n";
	// finish header guard
	out << "#endif" << '\n';
	return 0;
}