import re


def sortPatterns( pattern_list ):
  scoring_list = []
  for pattern in pattern_list:
    # this could be a black hole of optimization, this is just a decent aproxamazation
    wildcards_count = len( re.findall( r'\.\*|\.\+|\.\?|\[.*?\]|\(.*?\)|\.', pattern ) )
    literals_count = len( re.findall( r'[A-Za-z0-9]', pattern ) )

    # Return a tuple for sorting: (fewer wildcards, more literals, longer length)
    scoring_list.append( ( pattern, ( wildcards_count, -literals_count, -len( pattern ) ) ) )

  return [ i[0] for i in sorted( scoring_list, key=lambda i: i[1] ) ]


def findMatchs( target, pattern_list ):
  return [ i for i in pattern_list if ( i == target or re.match( i, target ) ) ]


def findStep( current_version, target_version, firmware_map ):
  from_version_patterns = [ i[ 'from' ] for i in firmware_map.values() ]
  matching_patterns = findMatchs( current_version, sortPatterns( from_version_patterns ) )
  for pattern in matching_patterns:
    for filename, vermap in firmware_map.items():  # TODO: this is an ugly solution that "mostly" works, one of these days....
      if vermap[ 'from' ] == pattern and vermap[ 'to' ] == target_version:  # TODO: if we can't find an immediate match, look to see if we can get there through another file
          return filename

  return None
