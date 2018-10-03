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

  if conn.getcode() in ( 301, 302 ):
    new_url = conn.getheader( 'Location' )
    print( 'Redirecting to "{0}"....'.format( new_url ) )
    if new_url is None or new_url == url:
      raise Exception( 'Got Redirect to same URL "{0}" to "{1}"'.format( url, new_url ) )

    return http_getfile( new_url, proxy )

  if conn.getcode() != 200:
    raise Exception( 'Error with request, HTTP Error "{0}"'.format( conn.getcode() ) )

  result = conn.read()
  conn.close()
  return result
