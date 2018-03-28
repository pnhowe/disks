import urllib2

def http_getfile( url, proxy=None ):
  print 'Retreiving "%s"...' % url

  if proxy:
    if proxy == 'local':
      opener = urllib2.build_opener( urllib2.ProxyHandler( {} ) ) # disable even the env http(s)_proxy
    else:
      opener = urllib2.build_opener( urllib2.ProxyHandler( { 'http': proxy, 'https': proxy } ) )
  else:
    opener = urllib2.build_opener( urllib2.ProxyHandler( { 'http': None, 'https': None } ) ) # use environ

  opener.addheaders = [( 'User-agent', 'plato-linux-installer' )]

  conn = opener.open( url )

  if conn.getcode() != 200:
    raise Exception( 'Error with request, HTTP Error %s' % conn.getcode() )

  result = conn.read()
  conn.close()
  return result
