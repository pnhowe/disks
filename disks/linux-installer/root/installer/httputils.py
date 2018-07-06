from urllib import request


class HTTPErrorProcessorPassthrough( request.HTTPErrorProcessor ):
  def http_response( self, request, response ):
    return response


def http_getfile( url, proxy=None ):
  print( 'Retreiving "{0}"...'.format( url ) )

  if proxy:
    if proxy == 'local':
      opener = request.build_opener( HTTPErrorProcessorPassthrough, request.ProxyHandler( {} ) )  # disable even the env http(s)_proxy
    else:
      opener = request.build_opener( HTTPErrorProcessorPassthrough, request.ProxyHandler( { 'http': proxy, 'https': proxy } ) )
  else:
    opener = request.build_opener( HTTPErrorProcessorPassthrough, request.ProxyHandler( { 'http': None, 'https': None } ) )  # use environ

  opener.addheaders = [ ( 'User-agent', 'bootabledisks-linux-installer' ) ]

  conn = opener.open( url )

  if conn.getcode() != 200:
    raise Exception( 'Error with request, HTTP Error "{0}"'.format( conn.getcode() ) )

  result = conn.read()
  conn.close()
  return result
