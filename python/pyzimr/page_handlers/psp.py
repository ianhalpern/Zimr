from mako.template  import Template
from mako.lookup    import TemplateLookup
from mako.runtime   import Context
from mako           import exceptions as mako_exceptions
from StringIO       import StringIO

def render( path, connection=None ):
	result = "";

	try:
		lookup = TemplateLookup(
			directories=['.'],
			module_directory='mako_cache/',
			output_encoding='utf-8',
			encoding_errors='replace'
		)
		template = Template( filename=path, module_directory='mako_cache/', lookup=lookup )
	except OSError:
		return "error 1"

	buf = StringIO( )
	ctx = Context( buf, connection=connection )

	try:
		#loadme and unload me are so the procs modules are in scope
		template.render_context( ctx )
	except:
		connection.response.setHeader( "Content-Type", "text/plain" )
		return mako_exceptions.text_error_template( ).render( )

	result = str( buf.getvalue( ) )

	if not result:
		connection.response.setHeader( "Content-Type", "text/plain" )
		return mako_exceptions.text_error_template( ).render( )
	else:
		return result