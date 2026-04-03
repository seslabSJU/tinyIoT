#	SDTPrinter.py
#
#	Print an SDT in various formats
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.

""" This module provides functions to print an SDT in various formats, including OPML, Markdown, OneM2M SVG, and JSON.
"""

from typing import Optional
from rich import print

from sdtv4.SDT4PrintOneM2MSVG import print4OneM2MSVG
from sdtv4.SDT4Templates import print4SDT
from sdtv4.SDT4oneM2MHelper import prepare4OneM2M
from sdtv4.SDT4PrintOPML import print4DomainOPML
from sdtv4.SDT4Classes import SDT4Domain, Options
import common.SDTHelper as helper


def printOPML(domain:SDT4Domain, options:Options) -> Optional[str]:
	"""	Print the SDT in OPML format.
	
		Args:
			domain: The SDT4Domain object to be printed.
			options: The options for printing the SDT.

		Returns:
			The OPML representation of the SDT as a string.
	"""
	return print4DomainOPML(domain, options)


def printMarkdown(domain:SDT4Domain, options:Options) -> Optional[str]:
	"""	Print the SDT in Markdown format.
	
		Args:
			domain: The SDT4Domain object to be printed.
			options: The options for printing the SDT.

		Returns:
			The Markdown representation of the SDT as a string.
	"""
	return print4SDT(domain, options)


def printOneM2MSVG(domain:SDT4Domain, directory:str, options:Options) -> None:
	"""	Print the SDT in OneM2M SVG format.
	
		Args:
			domain: The SDT4Domain object to be printed.
			directory: The directory where the SVG file will be saved.
			options: The options for printing the SDT.
	"""
	helper.makeDir(directory)
	prepare4OneM2M(domain)
	print4OneM2MSVG(domain, options, directory)


def printOneM2MXSD(domain:SDT4Domain, directory:str, options:Options) -> None:
	"""	Print the SDT in OneM2M XSD format.
	
		Args:
			domain: The SDT4Domain object to be printed.
			directory: The directory where the XSD file will be saved.
			options: The options for printing the SDT.
	"""
	modelVersion = options['modelversion']
	if modelVersion is None:
		print('[red]--modelVersion <version> must be specified')
		return

	helper.makeDir(directory)
	prepare4OneM2M(domain)
	print4SDT(domain, options, directory)


def printApJSON(domain:SDT4Domain, options:Options) -> Optional[str]:
	""" This function generates a JSON version of the SDT.

		Args:
			domain: The domain object to be printed.
			options: The options for printing the SDT.

		Returns:
			The JSON representation of the SDT as a string.
	"""
	prepare4OneM2M(domain)
	return print4SDT(domain, options)


	# TODO to file
	# print4SDT(domain, options, directory)