import sqlite3
import sys
import pickle
import base64
import os
import hashlib
import pwd
import grp
from datetime import datetime
from libconfig.jinja2 import FileSystemLoader, Environment, nodes
from libconfig.jinja2.ext import Extension, do as do_ext


def unique_list( value ):
  return set( value )

# Thought: what if there was a way to make sure a value is presnt, and if missing cause an error
# mabey a {% required myvalue, myothervalue %} type thing, otherwise all templates will have to
# fail sane, mabey there are times when that isn't approporate


class TargetWriter( Extension ):
  tags = set( [ 'target' ] )

  def __init__( self, environment ):
    super( TargetWriter, self ).__init__( environment )
    environment.globals[ '_target_list' ] = []
    environment.globals[ '_root_dir' ] = ''

  def parse( self, parser ):
    lineno = next( parser.stream ).lineno

    args = [ parser.parse_expression() ]

    if parser.stream.skip_if( 'comma' ):
      args.append( parser.parse_expression() )
      if parser.stream.skip_if( 'comma' ):
        args.append( parser.parse_expression() )
      else:
        args.append( nodes.Const( '0644' ) )

    else:
      args.append( nodes.Const( 'root.root' ) )
      args.append( nodes.Const( '0644' ) )

    body = parser.parse_statements( [ 'name:endtarget' ], drop_needle=True )
    return nodes.CallBlock( self.call_method( '_write', args ), [], [], body ).set_lineno( lineno )

  def _write( self, filename, owner, mode, caller ):
    self.environment.globals[ '_target_list' ].append( str( filename ) )
    if not self.environment.globals[ '_dry_run' ]:
      target_file = os.path.join( self.environment.globals[ '_root_dir' ], *( filename.split( '/' ) ) )

      if not os.path.exists( os.path.dirname( target_file ) ):
        os.makedirs( os.path.dirname( target_file ) )

      fd = open( target_file, 'w' )
      fd.write( caller() )
      fd.close()

      if os.getuid() == 0:
        ( user, group ) = owner.split( '.' )

        os.chown( target_file, pwd.getpwnam( user ).pw_uid, grp.getgrnam( group ).gr_gid )

        os.chmod( target_file, int( mode, 8 ) )
      else:
        print( 'WARNING: not running as root, unable to set owner nor mode of target file' )

    return "<{0} {1} {2}>".format( filename, owner, mode )


class Config():
  def __init__( self, provider, template_dir, config_db, root_dir, configurator ):  # root_dir should not affect template nor config_db paths
    super().__init__()
    self.provider = provider
    self.template_dir = template_dir
    self.root_dir = root_dir
    self.configurator = configurator
    self.extra_values = {}
    self._checkDB( config_db )  # check db before we connect to it
    self.conn = sqlite3.connect( config_db )

  def __del__( self ):
    self.conn.close()

  # utility functions
  def _checkDB( self, config_db ):
    conn = sqlite3.connect( config_db )
    cur = conn.cursor()
    cur.execute( 'SELECT COUNT(*) FROM "sqlite_master" WHERE type="table" and name="control";' )
    ( count, ) = cur.fetchone()
    if count == 0:
      conn.execute( 'CREATE TABLE "control" ( "key" text, "value" text );' )
      conn.commit()
      conn.execute( 'INSERT INTO "control" VALUES ( "version", "1" );' )
      conn.commit()

    cur = conn.cursor()
    cur.execute( 'SELECT "value" FROM "control" WHERE "key" = "version";' )
    ( version, ) = cur.fetchone()
    if version < '2':
      conn.execute( 'DROP TABLE IF EXISTS "templates";' )
      conn.commit()
      conn.execute( """CREATE TABLE "templates" (
      "package" char(50) NOT NULL,
      "template" char(50) NOT NULL,
      "templatehash" char(40),
      "target" char(200),
      "targethash" char(40),
      "lastChecked" datetime,
      "lastBuilt" datetime,
      "created" datetime DEFAULT CURRENT_TIMESTAMP,
      "modified" datetime DEFAULT CURRENT_TIMESTAMP
  );""" )
      conn.commit()
      conn.execute( 'DROP TABLE IF EXISTS "packages";' )
      conn.commit()
      conn.execute( """CREATE TABLE "packages" (
      "package" char(50) NOT NULL,
      "version" char(40),
      "lastChecked" datetime,
      "lastUpdated" datetime,
      "created" datetime DEFAULT CURRENT_TIMESTAMP,
      "modified" datetime DEFAULT CURRENT_TIMESTAMP
  );""" )
      conn.commit()
      conn.execute( 'DROP TABLE IF EXISTS "config_cache";' )
      conn.commit()
      conn.execute( """CREATE TABLE "config_cache" (
      "name" char(40) NOT NULL,
      "value" text,
      "created" datetime DEFAULT CURRENT_TIMESTAMP,
      "modified" datetime DEFAULT CURRENT_TIMESTAMP
  );""" )
      conn.commit()
      conn.execute( 'UPDATE "control" SET "value" = "2" WHERE "key" = "version";' )
      conn.commit()

    cur = conn.cursor()
    cur.execute( 'SELECT "value" FROM "control" WHERE "key" = "version";' )
    ( version, ) = cur.fetchone()
    if version < '3':
      conn.execute( 'DROP TABLE IF EXISTS "targets";' )
      conn.commit()
      conn.execute( """CREATE TABLE "targets" (
      "package" char(50) NOT NULL,
      "template" char(50) NOT NULL,
      "target" char(200),
      "targetHash" char(40),
      "lastChecked" datetime,
      "lastBuilt" datetime,
      "created" datetime DEFAULT CURRENT_TIMESTAMP,
      "modified" datetime DEFAULT CURRENT_TIMESTAMP
  );""" )

      conn.execute( 'DROP TABLE IF EXISTS "templates_new";' )
      conn.commit()
      conn.execute( """CREATE TABLE "templates_new" (
      "package" char(50) NOT NULL,
      "template" char(50) NOT NULL,
      "templateHash" char(40),
      "lastChecked" datetime,
      "lastBuilt" datetime,
      "created" datetime DEFAULT CURRENT_TIMESTAMP,
      "modified" datetime DEFAULT CURRENT_TIMESTAMP
  );""" )

      cur = conn.cursor()
      cur.execute( 'SELECT "package", "template", "templatehash", "target", "targethash", "lastChecked", "lastBuilt", "created", "modified" FROM "templates";' )
      for ( package, template, template_hash, target, target_hash, lastChecked, lastBuilt, created, modified ) in cur.fetchall():
        conn.execute( 'INSERT INTO "templates_new" ( "package", "template", "templateHash", "created", "modified" ) VALUES ( ?, ?, ?, ?, ? );', ( package, template, template_hash, created, modified ) )
        conn.execute( 'INSERT INTO "targets" ( "package", "template", "target", "targetHash", "created", "modified" ) VALUES ( ?, ?, ?, ?, ?, ? );', ( package, template, target, target_hash, created, modified ) )

      conn.commit()

      conn.execute( 'DROP TABLE "templates";' )
      conn.commit()

      conn.execute( 'ALTER TABLE "templates_new" RENAME TO "templates";' )
      conn.commit()

      conn.execute( 'UPDATE "control" SET "value" = "3" WHERE "key" = "version";' )
      conn.commit()

  def _updateConfigCache( self, new_values ):
    cache = self.getConfigCache()
    for name in new_values:
      value = base64.b64encode( pickle.dumps( new_values[name], pickle.HIGHEST_PROTOCOL ) )
      if name not in cache:
        self.conn.execute( 'INSERT INTO "config_cache" ( "name", "value" ) VALUES ( ?, ? );', ( name, value ) )

      elif cache[name] != value:
        self.conn.execute( 'UPDATE "config_cache" SET "value"=?, "modified"=CURRENT_TIMESTAMP WHERE "name"=?;', ( value, name ) )

    cur = self.conn.cursor()
    cur.execute( 'SELECT "name" FROM "config_cache";' )
    for ( name, ) in cur.fetchall():
      if name not in new_values:
        self.conn.execute( 'DELETE FROM "config_cache" WHERE "name"=?;', ( name, ) )

    cur.close()

    self.conn.commit()

  def _getPackageTemplates( self, package ):
    result = []
    for line in os.listdir( os.path.join( self.template_dir, package ) ):
      tmpfile = os.path.join( self.template_dir, package, line )
      if not os.path.isfile( tmpfile ) or not os.access( tmpfile, os.R_OK ):
        continue
      if line.endswith( '.tpl' ):
        result.append( line[:-4] )
    return result

  def _getTemplates( self, package ):
    result = {}
    cur = self.conn.cursor()
    cur.execute( 'SELECT "template", "templateHash" FROM "templates" WHERE package = "{0}" ORDER BY template;'.format( package ) )
    for ( template, template_hash ) in cur.fetchall():
      result[ str( template ) ] = { 'hash': str( template_hash ) }

    cur.close()

    return result

  def _getTargets( self, package, template ):
    result = {}
    cur = self.conn.cursor()
    cur.execute( 'SELECT "target", "targethash", "lastChecked", "lastBuilt" FROM "targets" WHERE package = "{0}" AND template = "{1}" ORDER BY target;'.format( package, template ) )
    for ( target, target_hash, checked, built ) in cur.fetchall():
      result[ str( target ) ] = { 'hash': str( target_hash ), 'last_checked': str( checked ), 'last_built': str( built ) }

    cur.close()

    return result

  def _renderTemplate( self, package, config, template, root_dir ):
    eng = Environment( loader=FileSystemLoader( os.path.join( self.template_dir, package ) ), extensions=[ TargetWriter, do_ext ] )
    eng.filters[ 'unique_list' ] = unique_list
    eng.globals.update( _dry_run=False )
    eng.globals.update( _root_dir=os.path.join( self.root_dir, root_dir ) )
    tmpl = eng.get_template( '{0}.tpl'.format( template ) )
    tmpl.render( config )
    return eng.globals[ '_target_list' ]

  def _targetFiles( self, package, config, template ):
    eng = Environment( loader=FileSystemLoader( os.path.join( self.template_dir, package ) ), extensions=[ TargetWriter, do_ext ] )
    eng.filters[ 'unique_list' ] = unique_list
    eng.globals.update( _dry_run=True )
    tmpl = eng.get_template( '{0}.tpl'.format( template ) )
    tmpl.render( config )
    return eng.globals[ '_target_list' ]

  def getMasterConfig( self ):
    values = self.provider.getConfig()

    if not values:
      print( 'Provider returned empty config - Device Not found.' )
      sys.exit( 1 )

    if self.provider.uuid is None or self.provider.id is None:
      print( 'Retrieved config is not valid' )
      sys.exit( 1 )

    return values

  def setOverlayValues( self, values ):
    self.extra_values.update( values )

  def getTemplateList( self, package ):
    return self._getPackageTemplates( package )

  def getTargetFiles( self, package, template, config=None ):
    if not config:
      config = self.getConfigCache()

    return self._targetFiles( package, config, template )

  def updateConfigCacheFromMaster( self ):
    self._updateConfigCache( self.getMasterConfig() )

  def getConfigCache( self ):
    result = {}
    cur = self.conn.cursor()
    cur.execute( 'SELECT "name", "value" FROM "config_cache";' )
    for ( name, value ) in cur.fetchall():
      result[ str( name ) ] = pickle.loads( base64.b64decode( value ) )

    cur.close()

    return result

  def getTargetStatus( self, package, config=None ):
    if config is None:
      config = self.getConfigCache()

    template_db_list = self._getTemplates( package )
    template_list = self._getPackageTemplates( package )
    package_dir = os.path.join( self.template_dir, package )

    results = []
    last_modified = None
    if 'last_modified' in config:
      last_modified = config[ 'last_modified' ]

    for template in template_list:
      target_list = self._targetFiles( package, config, template )
      target_db_list = self._getTargets( package, template )

      template_file = os.path.join( package_dir, '{0}.tpl'.format( template ) )
      template_hash = hashlib.sha1( open( template_file, 'r' ).read().encode() ).hexdigest()

      for target in target_list:
        target_file = os.path.join( self.root_dir, target )
        tmp = {}
        tmp[ 'template' ] = template
        tmp[ 'target' ] = target
        tmp[ 'need_update' ] = 'Unknown'
        tmp[ 'last_built' ] = 'Unknown'
        tmp[ 'refrenced' ] = 'Yes'

        if template not in template_db_list or target not in target_db_list:
          tmp[ 'need_update' ] = 'Yes'
          tmp[ 'localmod' ] = 'Unknown'

        else:
          if os.path.isfile( target_file ):
            target_hash = hashlib.sha1( open( target_file, 'r' ).read().encode() ).hexdigest()

            if target_db_list[ target ][ 'hash' ] != target_hash:
              tmp[ 'localmod' ] = 'Yes'
            else:
              tmp[ 'localmod' ] = 'No'

          else:
            tmp[ 'localmod' ] = 'Missing'

          tmp[ 'last_built' ] = target_db_list[ target ][ 'last_built' ]
          if not last_modified:
            tmp[ 'need_update' ] = 'Unknown'
          elif last_modified > tmp[ 'last_built' ] or template_hash != template_db_list[ template ][ 'hash' ]:
            tmp[ 'need_update' ] = 'Yes'
          else:
            tmp[ 'need_update' ] = 'No'

        results.append( tmp )

    for template in template_db_list:
      target_db_list = self._getTargets( package, template )
      if template not in template_list:
        for target in target_db_list:
          tmp = {}
          tmp[ 'template' ] = template
          tmp[ 'target' ] = target
          tmp[ 'need_update' ] = 'Unknown'
          tmp[ 'last_built' ] = target_db_list[ target ][ 'last_built' ]
          tmp[ 'refrenced' ] = 'No'
          tmp[ 'localmod' ] = 'Unknown'
          results.append( tmp )

      else:
        target_list = self._targetFiles( package, config, template )
        for target in target_db_list:
          if target not in target_list:
            tmp = {}
            tmp[ 'template' ] = template
            tmp[ 'target' ] = target
            tmp[ 'need_update' ] = 'Unknown'
            tmp[ 'last_built' ] = target_db_list[ target ][ 'last_built' ]
            tmp[ 'refrenced' ] = 'No'
            tmp[ 'localmod' ] = 'Unknown'
            results.append( tmp )

    return results

  def getPackageList( self ):
    result = []
    for package in os.listdir( self.template_dir ):
      if not os.path.isdir( os.path.join( self.template_dir, package ) ):
        continue
      result.append( package )

    return result

  def configPackage( self, package, master_template_list, force, dry_run, no_backups, re_generate, dest_rootdir='', config=None ):
    if config is None:
      config = self.getConfigCache()

    config = config.copy()  # just incase, we don't want to mess up what we were passed
    config.update( self.extra_values )
    config[ '__configurator__' ] = self.configurator

    last_modified = '0'  # TODO: treat last_modified as a actuall dateetime
    if 'last_modified' in config:
      last_modified = config[ 'last_modified' ]

    template_list = self._getPackageTemplates( package )
    template_db_list = self._getTemplates( package )
    package_dir = os.path.join( self.template_dir, package )

    for template in template_list:
      if template not in master_template_list:
        continue

      template_file = os.path.join( package_dir, '{0}.tpl'.format( template ) )
      if not os.path.isfile( template_file ):
        print( 'Template "{0}" for package "{1}" is not found, skipped...'.format( template, package ) )
        continue

      template_hash = hashlib.sha1( open( template_file, 'r' ).read().encode() ).hexdigest()

      target_list = self._targetFiles( package, config, template )
      target_db_list = self._getTargets( package, template )

      need_build = False
      no_build = False

      for target in target_list:
        if dest_rootdir:
          target_file = os.path.join( self.root_dir, *( dest_rootdir.split( '/' ) + target.split( '/' ) ) )
        else:
          target_file = os.path.join( self.root_dir, *( target.split( '/' ) ) )

        if os.path.isfile( target_file ):
          target_hash = hashlib.sha1( open( target_file, 'r' ).read().encode() ).hexdigest()
        else:
          target_hash = None

        if target_hash is None:
          need_build = True

        if target in target_db_list:
          if last_modified > target_db_list[ target ][ 'last_built' ]:
            need_build = True

          if not target_hash:
            msg = None  # it's missing, quietly rebuild
            need_build = True
          elif target_hash != target_db_list[ target ][ 'hash' ]:
            msg = 'has local changes'
            need_build = True
          else:
            msg = None  # all good

        elif target_hash:
          msg = 'allready exists'
        else:
          msg = None  # file dosne't allready exist ( no target_hash ) and isn't allready in the db, so slightly create it

        if need_build and msg:
          if not force:
            print( 'Target "{0}" for Template "{1}" in package "{2}" {3}, skipped.'.format( target, template, package, msg ) )
            no_build = True

          elif no_backups:
            print( 'Target "{0}" for Template "{1}" in package "{2}" {3}, forcefully overwritten.'.format( target, template, package, msg ) )

          else:
            print( 'Target "{0}" for Template "{1}" in package "{2}" {3}, old file saved.'.format( target, template, package, msg ) )
            if not dry_run:
              os.rename( target_file, '{0}.{1}-{2}'.format( target_file, self.configurator, datetime.utcnow().strftime( '%Y-%m-%d-%H-%M' ) ) )

      if no_build:
        print( 'Template "{0}" skipped.'.format( template ) )
        continue

      if template not in template_db_list:
        print( 'Template "{0}" in "{1}" is new, building.'.format( template, package ) )
        need_build = True

      elif template_hash != template_db_list[ template ][ 'hash' ]:
        print( 'Template "{0}" in "{1}" has been modified, rebuilding.'.format( template, package ) )
        need_build = True

      if not need_build and re_generate:
        print( 'Template "{0}" in "{1}" rebuild not required, but re-generate specified, rebuilding.'.format( template, package ) )
        need_build = True

      if dry_run:
        continue

      if not need_build and not dest_rootdir:
        if template not in template_db_list:
          self.conn.execute( 'INSERT INTO "templates" ( "package", "template", "lastChecked" ) VALUES ( ?, ?, CURRENT_TIMESTAMP );', ( package, template ) )
          for target in target_list:
            self.conn.execute( 'INSERT INTO "targets" ( "package", "template", "target", "lastChecked" ) VALUES ( ?, ?, ?, CURRENT_TIMESTAMP );', ( package, template, target ) )

        else:
          self.conn.execute( 'UPDATE "templates" SET "lastChecked"=CURRENT_TIMESTAMP, "modified"=CURRENT_TIMESTAMP WHERE "package"=? AND "template"=?;', ( package, template ) )
          for target in target_list:
            if target not in target_db_list:
              self.conn.execute( 'INSERT INTO "targets" ( "package", "template", "target", "lastChecked" ) VALUES ( ?, ?, ?, CURRENT_TIMESTAMP );', ( package, template, target ) )
            else:
              self.conn.execute( 'UPDATE "targets" SET "lastChecked"=CURRENT_TIMESTAMP, "modified"=CURRENT_TIMESTAMP WHERE "package"=? AND "template"=? AND "target"=?;', ( package, template, target ) )

        self.conn.commit()
        continue

      if not dest_rootdir:
        print( '(Re)Building "{0}" in "{1}"...'.format( template, package ) )

      self._renderTemplate( package, config, template, dest_rootdir )

      if dest_rootdir:
        continue

      if template in template_db_list:
        self.conn.execute( 'UPDATE "templates" SET "lastChecked"=CURRENT_TIMESTAMP, "templatehash"=?, "lastBuilt"=CURRENT_TIMESTAMP, "modified"=CURRENT_TIMESTAMP WHERE "package"=? AND "template"=?;', ( template_hash, package, template ) )
      else:
        self.conn.execute( 'INSERT INTO "templates" ( "templatehash", "package", "template", "lastChecked", "lastBuilt" ) VALUES ( ?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP );', ( template_hash, package, template ) )

      for target in target_list:
        target_hash = hashlib.sha1( open( os.path.join( self.root_dir, *( target.split( '/' ) ) ), 'r' ).read().encode() ).hexdigest()

        if template in template_db_list:
          self.conn.execute( 'UPDATE "targets" SET "lastChecked"=CURRENT_TIMESTAMP, "targethash"=?, "lastBuilt"=CURRENT_TIMESTAMP, "modified"=CURRENT_TIMESTAMP WHERE "package"=? AND "template"=? AND "target"=?;', ( target_hash, package, template, target ) )
        else:
          self.conn.execute( 'INSERT INTO "targets" ( "targethash", "package", "template", "target", "lastChecked", "lastBuilt" ) VALUES ( ?, ?, ?, ?, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP );', ( target_hash, package, template, target ) )

      self.conn.commit()
