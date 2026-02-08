#	SDTAbbreviate.py
#
#	Handle shortname
# 
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.

""" This module provides functions to handle shortnames for SDT elements.
	It includes functions to shortname names, add new shortnames, read and write shortnames from/to files,
	and manage occurrences of shortnames in the SDT structure.
"""

import csv, os
from typing import Tuple, Optional
from rich import print, get_console
from enum import IntEnum

Shortnames = dict[str, str]	# Dictionary for shortnames (name, abbr)
""" A dictionary that maps long names to their corresponding shortnames."""

shortnames:Shortnames = {}
"""A global dictionary that stores shortnames for long names."""

# Shortname that are newly created while processing
newShortname:Shortnames = {}
"""A dictionary that stores newly created shortnames during processing."""

# Dicionary for existing shortnames
preDefinedShortnames:Shortnames = {}
"""A dictionary that stores predefined shortnames, typically loaded from a file."""

# Dictionary for mapping of short names to places where used
# shortname -> (longname, [ occursIn ])
shortnameOccursIn:dict[str, Tuple[str, list[str]]] = {}
"""A dictionary that maps short names to tuples containing the long name and a list of places where the shortname occurs."""

class ElementType(IntEnum):
	""" Enum for element types in the SDT structure.
		This enum defines the types of elements that can be shortnamed in the SDT structure.
	"""
	moduleClass = 1
	""" Represents a module class in the SDT structure. """

	deviceClass = 2
	""" Represents a device class in the SDT structure. """

	subDeviceClass = 3
	""" Represents a sub-device class in the SDT structure. """

	action = 4
	""" Represents an action in the SDT structure. """

	moduleClassAttribute = 5
	""" Represents an attribute of a module class in the SDT structure. """

	deviceAttribute = 6
	""" Represents an attribute of a device in the SDT structure. """

	subDeviceAttribute = 7
	""" Represents an attribute of a sub-device in the SDT structure. """

	actionAttribute = 8
	""" Represents an attribute of an action in the SDT structure. """


# experimental shortname function. Move later
def toShortname(name:str, length:int=5, elementType:Optional[ElementType]=None, occursIn:Optional[str]=None) -> str:
	"""	Shorten a long name to a shorter form.
		This function follows the rules for shortname names in oneM2M TS-0023.

		Args:
			name: The long name to be shortnamed.
			length: The maximum length of the shortname (default is 5).
			elementType: The type of the element being shortnamed (optional).
			occursIn: The place where the shortname occurs (optional).

		Returns:
			The shortnamed name, ensuring it is unique and does not clash with existing shortnames.
	"""
	global shortnames
	result = ''

	def addOccursIn(sn:str) -> str:
		"""	Add occursIn to the list.
		"""

		if (a := shortnameOccursIn.get(sn)) is None:
			a = (name, [])
		if occursIn and occursIn not in a[1]:
			a[1].append(occursIn)
		shortnameOccursIn[sn] = a
		return sn

	if name in shortnames:			# return shortname if already in dictionary
		return addOccursIn(shortnames[name])
	if name in preDefinedShortnames:	# return shortname if it exists in the predefined dictionary
		return addOccursIn(preDefinedShortnames[name])

	l = len(name)

	# name is less of equal the max allowed length
	if l <= length:
		result = name
	else:	# length of the name is longer than the allowed name. So do some shortening
		# First char
		result  = name[0]
		# Last char
		result += name[-1]

		# Fill up the shortname
		if len(result) < length:
			mask = name[1:l-1]
			# Camel case chars
			camels = ''
			for i in range(1,l-1):
				c = name[i]
				if c.isupper():
					camels += c
					mask = mask[:i-1] + mask[i:]
			result = result[:1] + camels + result[1:]

			# Fill with remaining chars of the mask, starting from the back
			for i in range(0, length - len(result)):
				pos = i + 1
				result = result[:pos] + mask[i] + result[pos:] 

	# put the new shortname into the global dictionary.
	# resolve clashes
	abbr = result[:length]
	clashVal = -1
	while abbr in list(shortnames.values()) or abbr in list(preDefinedShortnames.values()):
		clashVal += 1
		prf = result[:length]
		pof = str(clashVal)
		abbr = prf[:len(prf)-len(pof)] + pof

	newShortname[name] = abbr
	return addOccursIn(abbr)


# Add a single shortname
def addShortname(name:str, shortname:str) -> None:
	"""	Add a single shortname to the global dictionary.

		Args:
			name: The long name to be shortnamed.
			shortname: The shortname for the long name.
	"""
	shortnames[name] = shortname


# Return shortnames
def getShortnames() -> Shortnames:
	"""	Return the current shortnames dictionary.

		Returns:
			A copy of the global shortnames dictionary.
	"""
	return shortnames.copy()


# Return new shortnames
def getNewShortnames() -> Shortnames:
	"""	Return the newly created shortnames dictionary.

		Returns:
			A copy of the global newShortname dictionary.
	"""
	return newShortname.copy()


# Return an shortname for a longName
def getShortname(name:str) -> Optional[str]:
	"""	Get the shortname for a given long name.

		Args:
			name: The long name for which the shortname is requested.

		Returns:shortname
			The shortname if it exists, otherwise None.
	"""
	if name in shortnames:
		return shortnames[name]
	if name in preDefinedShortnames:
		return preDefinedShortnames[name]
	return None


# Read already existing shortnames
def readShortnames(inFileName:str, predefined:bool=True) -> None:
	"""	Read shortnames from a CSV file and store them in the global dictionary.
	
		Args:
			inFileName: The name of the input file containing shortnames.
			predefined: If True, load into preDefinedShortnames, otherwise into shortnames.
	"""
	global preDefinedShortnames, shortnames
	if inFileName == None:
		return
	try:
		with open(inFileName) as csvfile:
			reader = csv.reader(csvfile, delimiter=',', quotechar='\\')
			for row in reader:
				match len(row):
					case 0:
						continue	# ignore empty rows
					case 2:				
						if predefined:
							preDefinedShortnames[row[0]] = row[1]
						else:
							shortnames[row[0]] = row[1]
					case _:
						print('[yellow]Unknown row found (ignored): ' + ', '.join(row))

	# 			if not len(row):	# ignore empty rows
	# 				continue
	# 			elif len(row) == 2:
	# 				if predefined:
	# 					preDefinedShortnames[row[0]] = row[1]
	# 				else:
	# 					shortnames[row[0]] = row[1]
	# 			else:
	# 				print('[yellow]Unknown row found (ignored): ' + ', '.join(row))
	
	except FileNotFoundError as e:
		print(f'[yellow]WARNING: No such file or directory: "{inFileName}": shortname input file ignored')
	except Exception as e:
		raise


def exportShortnames(shortnames:Shortnames, csvFileName:Optional[str]=None, mapFileName:Optional[str]=None, domain:Optional[str]=None) -> None:
	"""	Export the shortnames to a CSV file and/or a Python map file.

		Args:
			shortnames: The dictionary of shortnames to export.
			csvFileName: The name of the CSV file to write the shortnames to (optional).
			mapFileName: The name of the Python map file to write the shortnames to (optional).
	"""

	# don't export if there are no shortnames
	if not shortnames:
		return

	# Export as python map
	try:
		if mapFileName:
			with open(mapFileName, 'w') as f:
					f.write(str(shortnames))
	except IOError as err:
		print(f'[red]{err}')

	# Export as CSV
	try:
		if csvFileName:

			# Add domain to filename if provided
			if domain:
				base, ext = os.path.splitext(csvFileName)
				csvFileName = f"{base}_{domain}{ext}"

			with open(csvFileName, 'w', newline='') as f:
				writer = csv.writer(f)
				for key in sorted(shortnames.keys()):
					writer.writerow([key, shortnames[key]])

	except IOError as err:
		print(f'[red]{err}')
	except Exception as err:
		print(f'[red]{err}')


def importOccursIn(occursInFile:str='occursIn.csv') -> None:
	"""	Read occurrences of shortnames from a CSV file and store them in the global dictionary.

		Args:
			occursInFile: The name of the input file containing occurrences of shortnames.
	"""
	try:
		with open(occursInFile) as csvfile:
			reader = csv.reader(csvfile, delimiter=',')
			for row in reader:
				match len(row):
					case 0:
						continue	# ignore empty rows
					case 3:
						shortnameOccursIn[row[2]] = (row[0], [ s for s in row[1].split(', ') ])
					case _:
						print(f'[yellow]Unknown row found (ignored): {", ".join(row)} ({len(row)})')

				# if not len(row):	# ignore empty rows
				# 	continue
				# elif len(row) == 3:
				# 	shortnameOccursIn[row[2]] = (row[0], [ s for s in row[1].split(', ') ]) 
				# else:
				# 	print(f'[yellow]Unknown row found (ignored) : {", ".join(row)} ({len(row)})')
	except FileNotFoundError as e:
		print(f'[yellow]WARNING: No such file or directory: "{occursInFile}": occursIn input file ignored, but will be created on export')
	except Exception as e:
		print(f'[red]{e}')
		raise


def exportOccursIn(occursInFile:str='occursIn.csv') -> None:
	"""	Export occurrences of shortnames to a CSV file.

		Args:
			occursInFile: The name of the CSV file to write occurrences of shortnames to.
	"""
	if not shortnameOccursIn:
		return
	# Export as CSV
	try:
		if occursInFile:
			with open(occursInFile, 'w', newline='') as f:
				writer = csv.writer(f, delimiter=',')
				for key in sorted(shortnameOccursIn.keys()):
					_n, _l = shortnameOccursIn[key]
					# clear up the reference list
					_l = [	_t 
							for _t in _l
							if _t is not None and len(_t) and _t != _n
						]
					writer.writerow([ _n, ', '.join(_l), key ])
	except IOError as err:
		print(f'[red]{err}')
		get_console().print_exception(show_locals = True)
	except Exception as err:
		print(f'[red]{err}')
		get_console().print_exception(show_locals = True)