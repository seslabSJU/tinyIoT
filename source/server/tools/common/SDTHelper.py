#	SDTHelper.py
#
#	Helpers
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.


"""This module provides helper functions for handling SDT (Smart Device Template) elements, including sanitizing names, generating versioned filenames, and managing indentation for output formatting.
"""

from typing import Optional, Generator
import os, pathlib, time, datetime, textwrap, argparse
from enum import IntEnum, auto
from pathlib import Path


class OutType(IntEnum):
	""" Enum for output types in the SDT structure.
		This enum defines the types of output that can be generated from the SDT structure.
	"""
	action = auto()
	""" Represents an action in the SDT structure. """

	device = auto()
	""" Represents a device in the SDT structure. """
	
	enumeration = auto()
	""" Represents an enumeration in the SDT structure. """
	
	moduleClass = auto()
	""" Represents a module class in the SDT structure. """

	shortName = auto()
	""" Represents a short name in the SDT structure. """

	subDevice = auto()
	""" Represents a sub-device in the SDT structure. """

	unknown = auto()
	""" Represents an unknown type in the SDT structure. """

	def prefix(self) -> str:
		""" Return a prefix for the output type.
			This is used to generate filenames or identifiers based on the output type.

			Returns:
				A string representing the prefix for the output type.
		"""
		return {
			self.action.value: 'act-',
			self.device.value: 'dev-',
			self.enumeration.value: '',
			self.moduleClass.value: 'mod-',
			self.shortName.value: 'snm',
			self.subDevice.value: 'sub-',
			self.unknown.value: '',
		}[self.value]

	
# Sanitize a name 
def sanitizeName(name:Optional[str], isClass:bool) -> str:
	"""	Sanitize a name by removing unwanted characters and adjusting the case.

		Args:
			name: The name to be sanitized. If None, an empty string is returned.
			isClass: A boolean indicating whether the name is for a class (True) or not (False).

		Returns:
			A sanitized version of the name, with unwanted characters removed and the first character adjusted based on isClass.
	"""
	if not name:
		return ''
	result = name
	result = f'{result[0].upper() if isClass else result[0].lower()}{name[1:]}'
	return result.replace(' ', '')\
				 .replace('/', '')\
				 .replace('.', '')\
				 .replace(' ', '')\
				 .replace("'", '')\
				 .replace('´', '')\
				 .replace('`', '')\
				 .replace('(', '_')\
				 .replace(')', '_')\
				 .replace('-', '_')


# Sanitize the package name
def sanitizePackage(package:str) -> str:
	"""	Sanitize a package name by replacing slashes with dots.

		Args:
			package: The package name to be sanitized.

		Returns:
			A sanitized version of the package name, with slashes replaced by dots.
	"""
	return  package.replace('/', '.')


# get a versioned filename
def getVersionedFilename(fileName:str, 
						 extension:str='', 
						 name:Optional[str]=None, 
						 path:Optional[str]=None, 
						 outType:OutType=OutType.device, 
						 modelVersion:Optional[str]=None,
						 namespacePrefix:Optional[str]=None) -> str:
	"""	Generate a versioned filename based on the provided parameters.

		Args:
			fileName: The base name of the file.
			extension: The file extension (default is an empty string).
			name: An optional name to be included in the filename.
			path: An optional path where the file will be saved.
			outType: The type of output, used to determine the prefix (default is OutType.device).
			modelVersion: An optional model version to be included in the filename.
			namespacePrefix: An optional namespace prefix to be included in the filename.

		Returns:
			A string representing the full filename, including the prefix, sanitized name, and postfix.
	"""
	prefix  = ''
	postfix = ''
	if name:
		prefix += f'{sanitizeName(name, False)}_'
	else:
		if namespacePrefix:
			prefix += namespacePrefix.upper() + '-'
			if fileName.startswith(namespacePrefix+':'):
				fileName = fileName[len(namespacePrefix)+1:]
		prefix += f'{outType.prefix()}'

	if modelVersion:
		postfix += f'-v{modelVersion.replace(".", "_")}'

	fullFilename = ''
	if path:
		fullFilename = path + os.sep
	fullFilename += f'{prefix}{sanitizeName(fileName, False)}{postfix}'
	if extension:
		fullFilename += f'.{extension}'

	return fullFilename


def makeDir(directory:str, parents:bool=True) -> Path:
	"""	Create a directory including missing parents.
		If the directory exists then this is ignored.

		Args:
			directory: The path of the directory to be created.
			parents: If True, create parent directories as needed (default is True).

		Returns:
			The path object of the new directory
	"""
	try:
		path = pathlib.Path(directory)
		path.mkdir(parents=parents)
	except FileExistsError:
		# ignore existing directory for now
		pass
	return path


# Create package path and make directories
# def getPackage(directory:str, domain) -> tuple[str, Path]:
# 	path = makeDir(directory)
# 	return sanitizePackage(domain.id), path


# Export the content for a ModuleClass or Device
def exportArtifactToFile(name:str, path:str, extension:str, content:str, outType:OutType=OutType.moduleClass) -> None:
	"""	Export the content to a file with a versioned filename.

		Args:
			name: The name of the artifact to be exported.
			path: The path where the file will be saved.
			extension: The file extension for the exported file.
			content: The content to be written to the file.
			outType: The type of output, used to determine the prefix for the filename (default is OutType.moduleClass).
	"""
	fileName = getVersionedFilename(name, extension, path=str(path), outType=outType)
	outputFile = None
	try:
		with open(fileName, 'w') as outputFile:
			outputFile.write(content)		
	except IOError as err:
		print(err)


# Get a timestamp
def getTimeStamp() -> str:
	"""	Get a timestamp in the format 'YYYY-MM-DD-HH-MM-SS'.

		Returns:
			A string representing the current timestamp in the specified format.
	"""
	return datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d-%H-%M-%S')


def deleteEmptyFile(filename:str) -> None:
	"""	Delete a file if it is empty.

		Args:
			filename: The name of the file to be checked and potentially deleted.
	"""
	if os.stat(filename).st_size == 0:
		os.remove(filename)  

#############################################################################
#
#	Tabulator handling
# tabulator level
tab = 0
""" Current tab level for indentation.
"""

tabChar = '\t'
""" Character used for indentation in the output.
"""

def incTab() -> None:
	"""	Increase the tab level by one.
		This is used to adjust the indentation level for output formatting.
	"""
	global tab
	tab += 1


def decTab() -> None:
	"""	Decrease the tab level by one.
		This is used to adjust the indentation level for output formatting.
	"""
	global tab
	if tab > 0:
		tab -= 1


def setTabChar(val:str) -> None:
	"""	Set the tab character used for indentation.
		This is used to format output with the correct indentation.

		Args:
			val: A string representing the tab character to be used.
	"""
	global tabChar
	tabChar = val


def getTabIndent() -> str:
	"""	Return the current tab indentation as a string.
		This is used to format output with the correct indentation.

		Returns:
			A string containing the current tab indentation.
	"""
	return ''.join(tabChar for _ in range(tab))


def newLine() -> str:
	"""	Return a new line with the current tab indentation.
		This is used to format output with the correct indentation.

		Returns:
			A string containing a newline character followed by the current tab indentation.
	"""
	return f'\n{getTabIndent()}'


#
#	Helper methods for argument parsing
#

def convertArgLineToArgs(argLine:str) -> Generator[str, None, None]:
	"""	Convert single lines to arguments. Deliver one at a time.
		Skip empty lines.

		Args:
			argLine: A string containing arguments separated by spaces.

		Yields:
			Each argument as a separate string, skipping any empty arguments.
	"""
	for arg in argLine.split():
		if not arg.strip():
			continue
		yield arg


class MultilineFormatter(argparse.HelpFormatter):
	"""	Formatter for argparse.
	"""

	def _fill_text(self, text:str, width:int, indent:str) -> str:
		"""	Format the text to fit within the specified width.
			Replace multiple whitespaces with a single space.
			Wrap text to the specified width with indentation.

			Args:
				text: The text to format.
				width: The maximum width of the formatted text.
				indent: The indentation to apply to each line.

			Returns:
				The formatted text with wrapped lines and indentation.

		"""
		text = self._whitespace_matcher.sub(' ', text).strip()
		paragraphs = text.split('|n ')
		multiline_text = ''
		for paragraph in paragraphs:
			formatted_paragraph = textwrap.fill(paragraph, width, initial_indent=indent, subsequent_indent=indent) + '\n'
			multiline_text = multiline_text + formatted_paragraph
		return multiline_text