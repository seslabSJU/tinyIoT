#	SDTSVG.py
#
#	SVG Helper for SDTTool
#
#	(c) 2015 by Andreas Kraft
#	License: Apache 2. See the LICENSE file for further details.


""" This module provides classes and functions to create SVG representations of Smart Device Templates (SDT).
"""

from __future__ import annotations
from typing import Optional

stdHeight=30
""" The height of a standard resource or attribute in the SVG representation. """

stdWidth=220
""" The width of a standard resource or attribute in the SVG representation. """

sdtBorderSpace=10
""" The space around the border of the SVG representation. """

stdGapH=50
""" The horizontal gap between resources or attributes in the SVG representation. """

stdGapV=20
""" The vertical gap between resources or attributes in the SVG representation. """

stdRound=15
""" The radius for rounded corners in the SVG representation. """

stdFontSize=12
""" The font size for text in the SVG representation. """

sdtFontSizeLine=10
""" The font size for line labels in the SVG representation. """

stdFontFamily='sans-serif'
""" The font family for text in the SVG representation. """

stdFontColor='black'
""" The color of the font for text in the SVG representation. """

stdStrokeColor='black'
""" The color of the stroke for shapes in the SVG representation. """

stdStrokeWidth=1
""" The width of the stroke for shapes in the SVG representation. """

stdFillColor='white'
""" The fill color for shapes in the SVG representation. """


class Resource():
	""" A class representing a resource in the SVG representation of an SDT.
	"""
	
	def __init__(self, name:str, x:float=sdtBorderSpace, y:float=sdtBorderSpace, cardinality:str='1', specialization:bool=False) -> None:
		""" Initialize a Resource object.

			Args:
				name: The name of the resource.
				x: The x-coordinate for the resource in the SVG representation.
				y: The y-coordinate for the resource in the SVG representation.
				cardinality: The cardinality of the resource (default is '1').
				specialization: Whether this resource is a specialization (default is False).
		"""
		super(Resource, self).__init__()
		
		self.x = x
		""" The x-coordinate of the resource in the SVG representation. """

		self.y = y
		""" The y-coordinate of the resource in the SVG representation. """

		self.name = name
		""" The name of the resource. """


		self.cardinality = cardinality
		""" The cardinality of the resource, e.g., '0,1', '1', '0,n', etc. """

		self.parent:Optional[Resource] = None
		""" The parent resource of this resource, if any. """

		self.children:list[Resource|Attribute] = []
		""" A list of child resources or attributes of this resource. """

		self.specialization = specialization
		""" Whether this resource is a specialization. """


	def add(self, obj:Resource|Attribute) -> None:
		""" Add a child resource or attribute to this resource.

			Args:
				obj: The child resource or attribute to be added.
		"""
		self.children.append(obj)
		obj.x = self.x + stdWidth/2 + stdGapH
		obj.y = self.y + (stdHeight + stdGapV) * len(self.children)
		obj.parent = self


	def draw(self) -> str:
		""" Draw the SVG representation of this resource and its children.

			Returns:
				A string containing the SVG representation of this resource and its children.
		"""	
		result = drawResource(self.x, self.y, self.name, self.specialization)
		if self.parent is not None:
			result += drawLine(self.parent.x, self.parent.y, self.x, self.y, self.cardinality)
		for o in self.children:
			result += o.draw()
		return result


	def height(self) -> float:
		""" Calculate the height of this resource and its children.

			Returns:
				The height of this resource and its children in the SVG representation.
		"""
		max = self.y + stdHeight + sdtBorderSpace
		if len(self.children) == 0:
			return max
		else:
			for c in self.children:
				h = c.height()
				if h > max:
					max = h
			return max


	def width(self) -> float:
		""" Calculate the width of this resource and its children.

			Returns:
				The width of this resource and its children in the SVG representation.
		"""
		max = self.x + stdWidth + sdtBorderSpace
		if len(self.children) == 0:
			return max
		else:
			for c in self.children:
				w = c.width()
				if w > max:
					max = w
			return max


class Attribute():
	""" A class representing an attribute in the SVG representation of an SDT.
	"""

	def __init__(self, name:str, x:float=sdtBorderSpace, y:float=sdtBorderSpace, cardinality:str='0,1') -> None:
		""" Initialize an Attribute object.

			Args:
				name: The name of the attribute.
				x: The x-coordinate for the attribute in the SVG representation.
				y: The y-coordinate for the attribute in the SVG representation.
				cardinality: The cardinality of the attribute (default is '0,1').
		"""
		super(Attribute, self).__init__()

		self.x = x
		""" The x-coordinate of the attribute in the SVG representation. """

		self.y = y
		""" The y-coordinate of the attribute in the SVG representation. """

		self.name = name
		""" The name of the attribute. """

		self.cardinality = cardinality
		""" The cardinality of the attribute, e.g., '0,1', '1', '0,n', etc. """

		self.parent:Optional[Resource] = None
		""" The parent resource of this attribute, if any. """
	

	def draw(self) -> str:
		""" Draw the SVG representation of this attribute.

			Returns:
				A string containing the SVG representation of this attribute.
		"""
		result = drawAttribute(self.x, self.y, self.name)
		if self.parent is not None:
			result += drawLine(self.parent.x, self.parent.y, self.x, self.y, self.cardinality)
		return result


	def height(self) -> float:
		""" Calculate the height of this attribute.

			Returns:
				The height of this attribute in the SVG representation.
		"""
		return self.y + stdHeight + sdtBorderSpace


	def width(self) -> float:
		""" Calculate the width of this attribute.

			Returns:
				The width of this attribute in the SVG representation.
		"""
		return self.x + stdWidth + sdtBorderSpace

###############################################################################

# SVG Stuff

def svgStart(width:float=0, height:float=0, header:Optional[str]=None) -> str:
	""" Start the SVG representation with the given width, height, and optional header.

		Args:
			width: The width of the SVG canvas (default is 0).
			height: The height of the SVG canvas (default is 0).
			header: An optional header comment to be included in the SVG (default is None).

		Returns:
			A string containing the starting SVG declaration and header comment if provided.
	"""
	result  = '<?xml version="1.0"?>\n'

	if header is not None and len(header)>0:
		result += f'\n<!--\n{sanitizeText(header)}\n-->\n\n'
	if width > 0 and height > 0:
		result += f'<svg height="{height}" width="{width}" xmlns="http://www.w3.org/2000/svg">\n'
	else:
		result += '<svg xmlns="http://www.w3.org/2000/svg">\n'
	result += f'<rect width="100%" height="100%" fill="{stdFillColor}" />\n'
	return result


def svgFinish() -> str:
	""" Finish the SVG representation by closing the SVG tag.

		Returns:
			A string containing the closing SVG tag.
	"""
	return '</svg>\n'


def svgRect(x:float, y:float, w:float, h:float, rxy:float=0) -> str:
	""" Create an SVG rectangle element.

		Args:
			x: The x-coordinate of the rectangle.
			y: The y-coordinate of the rectangle.
			w: The width of the rectangle.
			h: The height of the rectangle.
			rxy: The radius for rounded corners (default is 0, meaning no rounding).

		Returns:
			A string containing the SVG representation of the rectangle.
	"""
	result  = '<rect '
	result += f'x="{x}" y="{y}" '
	result += f'width="{w}" height="{h}" '
	if rxy != 0:
		result += f'rx="{rxy}" ry="{rxy}" '
	result += f'stroke="{stdStrokeColor}" fill="{stdFillColor}" stroke-width="{stdStrokeWidth}"'
	result += '/>\n'
	return result


def svgText(x:float, y:float, text:str, anchor:Optional[str]=None, fontsize:float=-1) -> str:
	""" Create an SVG text element.

		Args:
			x: The x-coordinate for the text.
			y: The y-coordinate for the text.
			text: The text content to be displayed.
			anchor: Optional anchor position for the text (default is None).
			fontsize: The font size for the text (default is -1, meaning use standard font size).

		Returns:
			A string containing the SVG representation of the text element.
	"""
	if fontsize == -1:
		fontsize = stdFontSize
	result  = '<text '
	result += f'x="{x}" y="{y}" '
	result += f'fill="{stdFontColor}" '
	result += f'font-family="{stdFontFamily}" '
	result += f'font-size="{fontsize}" '
	if anchor is not None:
		result += f'style="text-anchor: {anchor};"'
	result += f'>{text}</text>\n'
	return result


def svgLine(x1:float, y1:float, x2:float, y2:float, color:str='black') -> str:
	""" Create an SVG line element.

		Args:
			x1: The x-coordinate of the start point of the line.
			y1: The y-coordinate of the start point of the line.
			x2: The x-coordinate of the end point of the line.
			y2: The y-coordinate of the end point of the line.
			color: The color of the line (default is 'black').

		Returns:
			A string containing the SVG representation of the line element.
	"""
	result  = '<line '
	result += f'x1="{x1}" y1="{y1}" '
	result += f'x2="{x2}" y2="{y2}" '
	result += f'stroke="{color}" stroke-width="{stdStrokeWidth}" '
	result += '/>\n'
	return result


def drawAttribute(x:float, y:float, text:str) -> str:
	""" Draw an attribute in the SVG representation.

		Args:
			x: The x-coordinate for the attribute.
			y: The y-coordinate for the attribute.
			text: The text content of the attribute.

		Returns:
			A string containing the SVG representation of the attribute.
	"""
	result  = svgRect(x, y, stdWidth, stdHeight, stdRound)
	result += svgText(x+(stdWidth/2), y+((stdHeight+stdFontSize)/2-2), text, 'middle')
	return result


def drawResource(x:float, y:float, text:str, isSpecialization:bool) -> str:
	""" Draw a resource in the SVG representation.

		Args:
			x: The x-coordinate for the resource.
			y: The y-coordinate for the resource.
			text: The text content of the resource.
			isSpecialization: Whether this resource is a specialization.

		Returns:
			A string containing the SVG representation of the resource.
	"""
	label = f'[{text}]' if isSpecialization else f'&lt;{text}&gt;'
	result  = svgRect(x, y, stdWidth, stdHeight)
	result += svgText(x+(stdWidth/2), y+((stdHeight+stdFontSize)/2-2), label, 'middle')
	return result


def drawLine(x1:float, y1:float, x2:float, y2:float, label:str) -> str:
	""" Draw a line between two points in the SVG representation with cardinality.

		Args:
			x1: The x-coordinate of the start point of the line.
			y1: The y-coordinate of the start point of the line.
			x2: The x-coordinate of the end point of the line.
			y2: The y-coordinate of the end point of the line.
			label: The label for the cardinality at the end of the line.

		Returns:
			A string containing the SVG representation of the line and its cardinality.
	"""
	# Draw line to parent
	xs = x1 + stdWidth/2
	ys = y1 + stdHeight
	xm = xs
	ym = y2 + stdHeight/2
	xe = x2
	ye = ym
	result  = svgLine(xs, ys, xm, ym, stdStrokeColor)
	result += svgLine(xm, ym, xe, ye, stdStrokeColor)

	# Draw cardinality
	result += svgText(xe-5, ye-sdtFontSizeLine/2, label, 'end', sdtFontSizeLine)
	return result


def sanitizeText(text:str) -> str:
	""" Sanitize text for SVG by replacing special characters.

		Args:
			text: The text to be sanitized.

		Returns:
			A string with special characters replaced to ensure valid SVG output.
	"""	
	if text is None or len(text) == 0:
		return ''
	return text.replace('<', '&lt;').replace('>', '&gt;')
