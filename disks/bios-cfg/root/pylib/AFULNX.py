from subprocess import Popen, PIPE
import re
from BIOSCfg import BIOSCfg

# https://github.com/mrwnwttk/afulnx
# https://opensource.com/life/16/8/almost-open-bios-and-firmware-update-tips-linux-users
# http://jack0425.blogspot.com/2017/08/the-ami-bios-support-linux-bios-update.html

class AFULNXCfg( BIOSCfg ):
  def __init__( self, *args, **kwargs ):
    self.password = None

  def setPassword( self, password ):
    self.password = password
