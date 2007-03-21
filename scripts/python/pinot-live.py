#!/usr/bin/env python
#
# Copyright 2005-2007 Fabrice Colin
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

import gnome
import deskbar
import deskbar.Handler, deskbar
import gobject
from gettext import gettext as _

from os.path import expanduser
HOME = expanduser ("~/")

def _check_requirements ():
	try:
		import dbus
		try :
			if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
				import dbus.glib
		except:
			return (deskbar.Handler.HANDLER_IS_NOT_APPLICABLE, "Python dbus.glib bindings not found.", None)
		return (deskbar.Handler.HANDLER_IS_HAPPY, None, None)
	except:
		return (deskbar.Handler.HANDLER_IS_NOT_APPLICABLE, "Python dbus bindings not found.", None)
	
HANDLERS = {
	"PinotFileSearchHandler" : {
		"name": "Pinot",
		"description": _("Search documents indexed by Pinot"),
		"requirements" : _check_requirements,
	}
}

class PinotFileMatch (deskbar.Match.Match):
	def __init__(self, handler, name, url, mime_type, main_language, **args):
		deskbar.Match.Match.__init__ (self, handler, **args)
		# The mailbox scheme is specific to Pinot
		self.url = url
		url_scheme = self.url[:url.index("://")]
		if url_scheme == "mailbox":
			print "Mailbox hit: ", self.url
			self.url = "file://" + url[len("mailbox://"):url.index('?')]
			url_scheme = "file"

		# Replace HOME with ~
		file_path = self.url[self.url.index("://") + 3:]
		if url_scheme == "file":
			file_path = file_path.replace(HOME, "~/")
			print "File hit: ", file_path

		self.name = name + " (" + file_path + ")"
		self.mime_type = mime_type
		self.main_language = main_language
		self._icon = deskbar.Utils.load_icon_for_file(self.url)

	def get_verb(self):
		return "%(name)s"

	def action(self, text=None):
		print "Opening Pinot hit: ", self.url
		# Prefer deskbar's url_show is available
		try:
			deskbar.Utils.url_show (self.url)
		except AttributeError:
			gnome.url_show (self.url)

	def get_category (self):
		return "files"

class PinotFileSearchHandler(deskbar.Handler.SignallingHandler):
	def __init__(self):
		deskbar.Handler.SignallingHandler.__init__(self, ("system-search", "pinot"))

		self.query_string = ""
		self.results = []
		self.results_count = 10

		import dbus
		# We have to do this or it won't work
		if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
			import dbus.glib
		try:
			# Set up the dbus connection
			self.bus = dbus.SessionBus()
			self.proxy_obj = self.bus.get_object('de.berlios.Pinot', '/de/berlios/Pinot')
			self.pinot_iface = dbus.Interface(self.proxy_obj, 'de.berlios.Pinot')
		except Exception, msg:
			print 'Caught D-Bus exception: ', msg
		self.set_delay (500)

	def query (self, qstring, max):
		print "SimpleQuery: ", qstring
		doc_ids = []
		try :
			import dbus
			doc_ids = self.pinot_iface.SimpleQuery (qstring, dbus.UInt32(max))
		except Exception, msg:
			print 'Caught D-Bus exception: ', msg
		# Save the query's details
		self.query_string = qstring
		self.results = []
		self.results_count = len(doc_ids)
		try :
			# Get the details of each hit
			for doc_id in doc_ids:
				print "Hit on document ", doc_id
				self.pinot_iface.GetDocumentInfo (dbus.UInt32(doc_id),
					reply_handler=self.__receive_hits, error_handler=self.__receive_error)
		except Exception, msg:
			print 'Caught exception: ', msg

	def __receive_hits (self, fields):
		name = "Unknown"
		url = ""
		mime_type = "application/octet-stream"
		main_language = ""

		# Get the fields we need to build a match
		for field in fields:
			if field[0] == "caption":
				name = field[1]
			elif field[0] == "url":
				url = field[1]
			elif field[0] == "type":
				mime_type = field[1]
			elif field[0] == "language":
				main_language = field[1]

		self.results.append( PinotFileMatch(self, name, url, mime_type, main_language) )
		print "Got hit ", len(self.results)
		if len(self.results) == self.results_count:
			self.emit_query_ready(self.query_string, self.results)

	def __receive_error (self, error):
		print 'D-Bus error: ', error

