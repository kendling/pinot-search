#
# Copyright 2005-2008 Fabrice Colin
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

import cgi, re
import gnome
import deskbar
from deskbar.handlers.actions.OpenFileAction import OpenFileAction
from deskbar.handlers.actions.ActionsFactory import get_actions_for_uri
import gobject
from gettext import gettext as _
import dbus

HANDLERS = ['PinotFileSearchModule']

TYPES = {
    "Documents"    : {
        "name"    : ("caption",),
        "category": "documents",
        "extra": {"uri":("url",), "mime_type":("type",), "language":("language",), "mod_time":("modtime",), "size":("size",)},
        },
}

class PinotFileMatch(deskbar.interfaces.Match):
	def __init__(self, fields, **args):
		deskbar.interfaces.Match.__init__ (self)
		self.result = fields
		if fields.has_key("url"):
			self.url = fields["url"]
		url_scheme = self.url[:self.url.index("://")]
		# The mailbox scheme is specific to Pinot
		if url_scheme == "mailbox":
			mailbox_url = self.url
			print 'Mailbox hit: ', mailbox_url
			self.url = "file://" + mailbox_url[len("mailbox://"):mailbox_url.index('?')]
			url_scheme = "file"

		self.result["url"] = self.url
		print 'Action: ', self.result["caption"], " ", self.result["url"]

		# Snippet
		self.result["snippet"] = ""
		if fields.has_key("extract"):
			snippet = fields["extract"]
			if snippet != None and snippet != "":
				# Remove trailing whitespaces and escape '%'
				snippet.strip().replace("%", "%%")
				# Escape
				tmp = re.sub(r"<.*?>", "", snippet)
				tmp = re.sub(r"</.*?>", "", tmp)
				print 'Extract ', tmp
				try:
					self.set_snippet("\n%s" % cgi.escape(tmp))
				except AttributeError:
					print 'Snippets not available'

		if url_scheme == "file":
			print 'File hit'
			self.add_action(OpenFileAction(self.result["caption"], self.result["url"]))
		else:
			print 'Other hit'
			self.add_all_actions(get_actions_for_uri(self.result["url"]))

	def get_hash(self, text=None):
		return self.result["url"]

	def get_category (self):
		try:
			return TYPES["Documents"]["category"]
		except:
			return 'files'

	def get_name(self, text = None):
		return self.result["caption"]

class PinotFileSearchModule(deskbar.interfaces.Module):
	INFOS = {
			'icon': deskbar.core.Utils.load_icon('pinot'),
			'name': _('Pinot Search'),
			'description': _('Search with Pinot'),
			'version': deskbar.defs.VERSION,
			'categories': {
			'documents': {
				'name': _('My Documents'),
			},
		},
	}

	@staticmethod
	def has_prerequisites ():
		try:
			try :
				if getattr(dbus, 'version', (0,0,0)) >= (0,41,0):
					import dbus.glib

				# Can we rely on dbus activation ?
				bus = dbus.SessionBus()
				proxy_obj = bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus')
				dbus_iface = dbus.Interface(proxy_obj, 'org.freedesktop.DBus')
				activatables = dbus_iface.ListActivatableNames()
				if not 'de.berlios.Pinot' in activatables:
					PinotFileSearchModule.INSTRUCTIONS = ('D-Bus activation will not start the Pinot daemon')
					return False
			except:
				PinotFileSearchModule.INSTRUCTIONS = ('Python dbus.glib bindings not found.')
				return False
			return True
		except:
			PinotFileSearchModule.INSTRUCTIONS = ('Python dbus bindings not found.')
			return False

	def __init__(self):
		deskbar.interfaces.Module.__init__(self)
		self.query_string = ""
		self.matches = []
		self.matches_count = 10
		# Connect to dbus in initialize() and if need be, query()
		self.bus = self.proxy_obj = self.pinot_iface = None

	def initialize(self):
		try:
			print 'First time connection'
			self.bus = dbus.SessionBus()
			self.proxy_obj = self.bus.get_object('de.berlios.Pinot', '/de/berlios/Pinot')
			self.pinot_iface = dbus.Interface(self.proxy_obj, 'de.berlios.Pinot')
		except Exception, msg:
			print 'Caught exception: ', msg
		except:
			print 'Caught unexpected exception'

	def query (self, qstring):
		print "Query: ", qstring
		max = 10
		estimated_hits = 0
		# Do we need to set up the dbus connection ?
		if self.pinot_iface:
			try:
				print 'Getting statistics'
				self.pinot_iface.GetStatistics()
			except:
				print 'GetStatistics failed'
				self.pinot_iface = None
		if not self.pinot_iface:
			try:
				print 'Connecting'
				self.bus = dbus.SessionBus()
				self.proxy_obj = self.bus.get_object('de.berlios.Pinot', '/de/berlios/Pinot')
				self.pinot_iface = dbus.Interface(self.proxy_obj, 'de.berlios.Pinot')
			except Exception, msg:
				print 'Caught exception: ', msg
			except:
				print 'Caught unexpected exception'
				return
		# Run the query
		hits_list = dbus.Array([], signature='aa{ss}') 
		try :
			print 'Querying default engine'
			(estimated_hits, hits_list) = self.pinot_iface.Query('', '', qstring,
				dbus.UInt32(0), dbus.UInt32(max))
		except Exception, msg:
			print 'Caught exception (Query): ', msg
		# Save the query's details
		self.query_string = qstring
		self.matches = []
		self.matches_count = len(hits_list)
		try :
			print 'Got ', self.matches_count, ' hits off ', estimated_hits
			# Get the details of each hit
			for hit in hits_list:
				self.__receive_hits(hit)
		except Exception, msg:
			print 'Caught exception (GetDocumentInfo): ', msg

	def __receive_hits (self, fields_list):
		match_fields = {
        	    "name": "Unknown",
        	    "type": "application/octet-stream",
        	}

		# Get the fields we need to build a match
		for field in fields_list:
			match_fields[field[0]] = field[1]

		self.matches.append( PinotFileMatch(match_fields) )
		print 'Got hit ', len(self.matches)
		if len(self.matches) == self.matches_count:
			self._emit_query_ready(self.query_string, self.matches)

