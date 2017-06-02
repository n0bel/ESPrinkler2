xcopy data-uncompressed\* data\ /s /y
@rem you'll need to fix the next line for your gzip command
@rem Here's one: http://gnuwin32.sourceforge.net/packages/gzip.htm
..\..\..\Utility\gzip.exe -v -f ^
  data\*.html ^
  data\*.css ^
  data\*.js ^
  data\*-schema.json
