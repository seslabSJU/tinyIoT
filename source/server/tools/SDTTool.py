#	SDTTool.py
#
#	Main module for the SDTTool
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.

""" SDTTool - A tool to read and convert Smart Device Templates (SDT) to various formats.
"""

from typing import Tuple, Optional
from xml.etree.ElementTree import XMLParser, ParseError
from sdtv4 import SDT4Printer
from sdtv4.SDT4Parser import SDT4Parser
from sdtv4.SDT4Classes import SDT4Domain, Options
import common.SDTHelper as SDTHelper


import sys, traceback, argparse, os
from rich import print


version = '0.9'
""" Version of the SDTTool. """

# class LineNumberingParser(XMLParser):
#     def _start_list(self, *args, **kwargs):
#         # Here we assume the default XML parser which is expat
#         # and copy its element position attributes into output Elements
#         element = super(self.__class__, self)._start_list(*args, **kwargs)
#         element._start_line_number = self.parser.CurrentLineNumber
#         element._start_column_number = self.parser.CurrentColumnNumber
#         element._start_byte_index = self.parser.CurrentByteIndex
#         return element

#     def _end(self, *args, **kwargs):
#         element = super(self.__class__, self)._end(*args, **kwargs)
#         element._end_line_number = self.parser.CurrentLineNumber
#         element._end_column_number = self.parser.CurrentColumnNumber
#         element._end_byte_index = self.parser.CurrentByteIndex
#         return element


#
# Parse the data
#
def parseData(targetParser:SDT4Parser, data:str) -> Tuple[Optional[SDT4Domain], list[str]]:
	"""	Parse the data with the given parser and handle errors.

		Args:
			targetParser: An instance of SDT4Parser or a subclass to parse the data.
			data: The XML data to be parsed.

		Returns:
			A tuple containing the parsed SDT4Domain object and a list of name spaces found in the data.
			If parsing fails, the domain will be None and an error message will be printed.
	"""
	parser = XMLParser(target=targetParser)
	errormsg = ''
	try:
		try:
			parser.feed(data)
		except SyntaxError as err:
			errormsg = str(err)
			print(err)
		except:
			traceback.print_exc()
		finally:
			parser.close()
	except ParseError as err:
		formatted_e = errormsg
		line = int(formatted_e[formatted_e.find("line ") + 5: formatted_e.find(",")])
		column = int(formatted_e[formatted_e.find("column ") + 7:])
		split_str = data.split("\n")
		print(f'{split_str[line - 1]}\n{len(split_str[line - 1][0:column])*"-"}')

	return targetParser.domain, targetParser.nameSpaces


#
#	XML Reader
#
def readSDTXML(inFileName:str) -> Tuple[Optional[SDT4Domain], list[str]]:
	""" Read and parse an SDT XML file.

		Args:
			inFileName: The name of the input file to read.
		
		Returns:
			A tuple containing the parsed SDT4Domain object and a list of name spaces found in the data.
			If parsing fails, the domain will be None and an error message will be printed.
	"""
	# read the data from the input file
	with open(inFileName, 'r') as inputFile:
		data = inputFile.read()

	# Parse the data
	return parseData(SDT4Parser(), data)


#
#	Output
#
def outputResult(outFileName:str, result:Optional[str]=None) -> None:
	"""	Print the output to stdout or to a file.

		Args:
			outFileName: The name of the output file or directory.
			result: The result to be printed. If None, nothing is printed.
	"""
	if not result:
		return
	if not outFileName:
		print(result)
	else:
		path = os.path.dirname(outFileName)
		if path:
			os.makedirs(path, exist_ok=True)
		try:
			with open(outFileName, 'w') as outputFile:
				outputFile.write(result)
		except IOError as err:
			raise err


def checkForNamespace(nameSpaces:list[str], checkNameSpace:str) -> bool:
	"""	Check whether a name space can be found in a list of nameSpaces.

		Args:
			nameSpaces: A list of name spaces to check.
			checkNameSpace: The name space to check for.

		Returns:
			True if the name space is found, False otherwise.
	"""
	for ns in nameSpaces:
		if (ns.find(checkNameSpace) > -1):
			return True
	return False


def main(argv:list[str]) -> None:
	"""	Main function to parse command line arguments and process the SDT file.
		
		Args:
			argv: The command line arguments passed to the script.
	"""
	outFile = None

	# Read command line arguments
	
	parser = argparse.ArgumentParser(description=f'SDTTool {version} - A tool to read and convert Smart Device Templates.', 
								  	 epilog='Read arguments from one or more configuration files: @file1 @file2 ...',
									 fromfile_prefix_chars='@', 
									 formatter_class=SDTHelper.MultilineFormatter)
	parser.convert_arg_line_to_args = SDTHelper.convertArgLineToArgs # type: ignore[method-assign, assignment]

	parser.add_argument('-o', '--outfile', action='store', dest='outFile', required=True, help='The output file or directory for the result. The default is stdout')
	parser.add_argument('-of', '--outputformat', choices=('opml', 'markdown', 'onem2m-svg', 'onem2m-xsd', 'apjson'), action='store', dest='outputFormat', default='markdown', help='The output format for the result. The default is markdown')
	parser.add_argument('--hidedetails',  action='store_true', help='Hide the details of module classes and devices when printing documentation')
	parser.add_argument('--markdownpagebreak',  action='store_true', help='Insert page breaks before ModuleClasse and Device definitions.')
	parser.add_argument('--licensefile', '-lf', action='store', dest='licensefile', help='Add the text of license file to output files')

	oneM2MArgs = parser.add_argument_group('oneM2M sepcific')
	oneM2MArgs.add_argument('--domain',  action='store', dest='domain', help='Set the domain for the model')
	oneM2MArgs.add_argument('-ns', '--namespaceprefix',  action='store', dest='namespaceprefix', default='m2m', help='Specify the name space prefix for the model')
	oneM2MArgs.add_argument('--shortnamesinfile', '-abif',  action='store', dest='shortnamesinfile', help='Specify the file that contains a CSV table of alreadys existing shortnames.')
	oneM2MArgs.add_argument('--shortnamelength',  action='store', dest='shortnamelength', type=int, default=5, help='Specify the maximum length for shortnames. The default is 5.')
	oneM2MArgs.add_argument('--xsdtargetnamespace',  action='store', dest='xsdtargetnamespace', help='Specify the target namespace for the oneM2M XSD (a URI).')
	oneM2MArgs.add_argument('--modelversion', '-mv', action='store', dest='modelversion', help='Specify the version of the model.')
	oneM2MArgs.add_argument('--svg-with-attributes',  action='store_true', dest='svgwithattributes', help='Generate SVG for ModuleClass attributes as well.')
	oneM2MArgs.add_argument('--xsdnamespacemapping', '-xsdnsmap',  action='store', dest='xsdnamespacemapping', nargs='*', help='Specify the target namespace for the oneM2M XSD (a URI).')
	oneM2MArgs.add_argument('--cdtversion',  action='store', dest='cdtversion', help='Specify the version number of the oneM2M XSD.')

	requiredNamed = parser.add_argument_group('required arguments')
	requiredNamed.add_argument('-i', '--infile', action='store', dest='inFile', required=True, help='The SDT input file to parse')
	
	if len(sys.argv)==1:
		parser.print_help()
		sys.exit(1)

	args 		= parser.parse_args()
	inFile 		= args.inFile
	outFile 	= args.outFile
	
	moreOptions:Options = {}
	moreOptions['hideDetails'] = args.hidedetails
	moreOptions['pageBreakBeforeMCandDevices'] = args.markdownpagebreak
	moreOptions['markdownPageBreak'] = args.markdownpagebreak # renamed, therefore twice
	moreOptions['licensefile'] = args.licensefile
	moreOptions['domain'] = args.domain
	moreOptions['namespaceprefix'] = args.namespaceprefix
	moreOptions['shortnamesinfile'] = args.shortnamesinfile
	moreOptions['shortnamelength'] = args.shortnamelength
	moreOptions['xsdtargetnamespace'] = args.xsdtargetnamespace
	moreOptions['xsdnamespacemapping'] = args.xsdnamespacemapping
	moreOptions['modelversion'] = args.modelversion
	moreOptions['outputFormat'] = args.outputFormat
	moreOptions['svgwithattributes'] = args.svgwithattributes
	moreOptions['cdtversion'] = args.cdtversion


	# Read input file. Check for correct format

	try:
		domain, nameSpaces = readSDTXML(inFile)
	except IOError as err:
		print(f'[red]ERROR: Could not read input file "{inFile}".', file=sys.stderr)
		print(f'[red]{err}', file=sys.stderr)
		return
	if not domain:
		print('[red]ERROR: No domain found in input file.')
		return
	if not checkForNamespace(nameSpaces, 'http://www.onem2m.org/xml/sdt/4.0'):
		print('[red]ERROR: Namespace "http://www.onem2m.org/xml/sdt/4.0" not found in input file.')
		return

	# Output to destination format
	else:
		try:
			match args.outputFormat:
				case 'opml':
					outputResult(outFile, SDT4Printer.printOPML(domain, moreOptions))
				case 'markdown':
					outputResult(outFile, SDT4Printer.printMarkdown(domain, moreOptions))
				case 'onem2m-svg':
					SDT4Printer.printOneM2MSVG(domain, outFile, moreOptions)
				case 'onem2m-xsd':
					SDT4Printer.printOneM2MXSD(domain, outFile, moreOptions)
				case 'apjson':
					outputResult(outFile, SDT4Printer.printApJSON(domain, moreOptions))
		except Exception as err:
			print('[red]ERROR: An error occurred while processing the input file.', file=sys.stderr)
			print(f'[red]{err}', file=sys.stderr)
			import traceback
			traceback.print_exc()
			return


if __name__ == "__main__":
	main(sys.argv[1:])
