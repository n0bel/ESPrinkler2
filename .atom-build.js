/*
 config file for atom-build for compiling with the Arduino IDE.
 The settings at the top are pretty unique to me, I'm sure you'll need to edit
 Also I only tested on Windows
 */


var path_to_arduino = '..\\..\\..\\ides\\arduino-1.6.11\\arduino_debug.exe';
var path_to_mkspiffs = '..\\..\\..\\ides\\arduino-1.6.11\\portable\\packages\\esp8266\\tools\\mkspiffs\\0.1.2\\mkspiffs.exe';
var path_to_esptool = '..\\..\\..\\ides\\arduino-1.6.11\\portable\\packages\\esp8266\\tools\\esptool\\0.4.9\\esptool.exe';
var path_to_terminal = '..\\..\\..\\terminal\\termite.exe';
var path_to_terminal_ini = '..\\..\\..\\terminal\\termite.ini';
var path_to_gzip = '..\\..\\..\\Utility\\gzip.exe';
var port = 'COM4';
var terminalBaud = '74880';
var resetMethod = 'nodemcu'

var board = {
  package: 'esp8266',
  arch: 'esp8266',
  //board: 'generic',
  board: 'd1_mini',
  parameters: {
    UploadSpeed: 256000,
    CpuFrequency: 80,
    // Debug: 'Disabled',   // only for generic
    // DebugLevel: 'None____', // only for generic
    // FlashFreq: 40, // only for generic
    // FlashMode: 'dio', // only for generic
    FlashSize: '4M3M',  //ESP-12E or F   4Mbyte
    //FlashSize: '512K64',  //ESP-03 or -01   512Kbyte
    //FlashSize: '1M128',  //ESP-07    1MByte
    // ResetMethod: resetMethod
  }
};

var spiffs = {  spiStart: 0x100000,  spiEnd: 0x3FB000,  spiPage: 256,  spiBlock: 8192 };  // ESP-12E or F 3Mbyte spiffs
//var spiffs = {  spiStart: 0x6B000,  spiEnd: 0x7B000,  spiPage: 256,  spiBlock: 4096 };  // ESP-03 or -01 64Kbyte spiffs
//var spiffs = {  spiStart: 0xDB000,  spiEnd: 0xFB000,  spiPage: 256,  spiBlock: 4096 };  // ESP-07 128Kbyte spiffs



function arduinoBoardString(b)

{
  var s = b.package + ':' + b.arch + ':' + b.board + ':';
  var sp = '';
  var p = b.parameters;
  for (var key in p) {
    if (p.hasOwnProperty(key)) {
      sp += ',' + key + '=' + p[key];
    }
  }
  return s + sp.substring(1);
}

var xargs =  [ "--board", arduinoBoardString(board) , "--port", port, '-v', '--pref', 'build.path={PROJECT_PATH}/build', '{FILE_ACTIVE}' ];

var functionMatch = function(output) {
  const error = /^(..[^:]+):(\d+):\s+(\w+:\s.*)$/;
  const error2 = /^(..[^:]+):(\d+):(\d+):\s+(\w+:\s.*)$/;
  // this is the list of error matches that atom-build will process
  const array = [];
  // iterate over the output by lines
  output.split(/[\r\n]/).forEach(line => {
    // process possible error messages
    const error_match = error.exec(line);
    if (error_match)
    {
      // map the regex match to the error object that atom-build expects
      array.push({
        file: error_match[1].indexOf('.') > 1 ? error_match[1] : error_match[1] + '.ino',
        line: error_match[2],
        message: error_match[3]
      });
    }
    else
    {
      const error_match2 = error2.exec(line);
      if (error_match2)
      {
        // map the regex match to the error object that atom-build expects
        array.push({
          file: error_match2[1].indexOf('.') > 1 ? error_match2[1] : error_match2[1] + '.ino',
          line: error_match2[2],
          col: error_match2[3],
          message: error_match2[4]
        });
      }
    }
  });
  return array;
}


var terminal_start = function(buildOutcome, stdout, stderr)
{
  if (!buildOutcome) return false;
  let fs = require('fs');
  let tsettings =
    "[Settings]\n"+
    "Port="+port+"\n"+
    "Baud="+terminalBaud+"\n"+
    "DataBits=8\n"+
    "StopBits=1\n"+
    "Parity=0\n"+
    "Handshake=0\n"+
    "ForwardPort=\n"+
    "[Options]\n"+
    "Window=1355 101 496 364\n"+
    "";
  fs.writeFileSync(this.cwd+'/'+path_to_terminal_ini,tsettings);
  let child = require('child_process').spawn(
    'cmd',
    [ '/C', path_to_terminal],
    { cwd: this.cwd, env: this.env }
  );
  global.xyzzy_terminal_child = child;
  return true;
}

var terminal_kill = function()
{
  if (global.xyzzy_terminal_child) {
    global.xyzzy_terminal_child.removeAllListeners();
    require('child_process').exec(
      'taskkill /pid ' + global.xyzzy_terminal_child.pid + ' /T /F '
    );
    global.xyzzy_terminal_child = null;
    let fs = require('fs');
    if (fs.existsSync(this.cwd+'/'+path_to_terminal_ini))
      fs.unlinkSync(this.cwd+'/'+path_to_terminal_ini);
  }
}

module.exports = {
  cmd: path_to_arduino,
  args: [ "--verify" ].concat(xargs),
  name: 'compile',
  cwd: '{PROJECT_PATH}',
  sh: true,
  preBuild: terminal_kill,
  functionMatch: functionMatch,
  targets: {
    upload: {
      cmd: path_to_arduino,
      args: [ "--upload" ].concat(xargs),
      preBuild: terminal_kill,
      functionMatch: functionMatch
    },
    upload_then_terminal: {
      cmd: path_to_arduino,
      args: [ "--upload" ].concat(xargs),
      functionMatch: functionMatch,
      preBuild: terminal_kill,
      postBuild: terminal_start,
    },
    justupload: {
      cmd: path_to_esptool,
      preBuild: terminal_kill,
      args: [ '-vv', "-cd", resetMethod, "-cb", board.parameters.UploadSpeed, "-cp", port, '-ca', '0x00000', '-cf', "{PROJECT_PATH}/build/{FILE_ACTIVE_NAME}.bin" ]
    },
    spiffs:
    {
      cmd: 'echo Prep and Upload spiffs',
      preBuild: terminal_kill,
      args: [
        '&&', 'copy {PROJECT_PATH}\\data-uncompressed\\* {PROJECT_PATH}\\data\\',
        '&&', path_to_gzip, '-v -f {PROJECT_PATH}/data/*.html {PROJECT_PATH}/data/*.css {PROJECT_PATH}/data/*.js',
        '&&', path_to_gzip, '-v -f {PROJECT_PATH}/data/config-schema.json',
        '&&', path_to_mkspiffs, '-c', '{PROJECT_PATH}/data', '-p', spiffs.spiPage, '-b', spiffs.spiBlock, '-s',  (spiffs.spiEnd - spiffs.spiStart), '{PROJECT_PATH}/build/spiffs.bin',
        '&&', path_to_esptool, '-vv', "-cd", resetMethod, "-cb", board.parameters.UploadSpeed, "-cp", port, "-ca", '0x'+spiffs.spiStart.toString(16), "-cf", '{PROJECT_PATH}/build/spiffs.bin'
       ]
    },
    terminal:
    {
      preBuild: terminal_kill,
      cmd: 'echo Running Terminal',
      postBuild: terminal_start
    },
    reset_then_terminal:
    {
      preBuild: terminal_kill,
      cmd: path_to_esptool,
      args: [ '-vv', "-cd", resetMethod, "-cb", board.parameters.UploadSpeed, "-cp", port, '-cr' ],
      postBuild: terminal_start
    },
    clean: {
      cmd: 'del {PROJECT_PATH}\\build /s/q && rmdir {PROJECT_PATH}\\build /s/q'
    }
  }
};
