/*
 config file for atom-build for compiling with the Arduino IDE.
 The settings at the top are pretty unique to me, I'm sure you'll need to edit
 Also I only tested on Windows
 */
/* global atom */
/* eslint-env node, atomtest */

var pathToArduino = '..\\..\\..\\ides\\arduino-1.6.11\\';
var pathRunArduino = pathToArduino + 'arduino_debug.exe';
var pathRunMkspiffs = pathToArduino + 'portable\\packages\\esp8266\\tools\\mkspiffs\\0.1.2\\mkspiffs.exe';
var pathRunEsptool = pathToArduino + 'portable\\packages\\esp8266\\tools\\esptool\\0.4.9\\esptool.exe';
var pathRunTerminal = '..\\..\\..\\terminal\\termie.exe';
var pathRunGzip = '..\\..\\..\\Utility\\gzip.exe';
var port = 'COM3';
var terminalBaud = '74880';
var resetMethod = 'nodemcu';
var terminalProcess = null;

var board = {
  package: 'esp8266',
  arch: 'esp8266',
  // board: 'generic',
  board: 'd1_mini',
  parameters: {
    UploadSpeed: 256000,
    CpuFrequency: 80,
    // Debug: 'Disabled',   // only for generic
    // DebugLevel: 'None____', // only for generic
    // FlashFreq: 40, // only for generic
    // FlashMode: 'dio', // only for generic
    FlashSize: '4M3M'  // ESP-12E or F   4Mbyte
    // FlashSize: '512K64',  //ESP-03 or -01   512Kbyte
    // FlashSize: '1M128',  //ESP-07    1MByte
    // ResetMethod: resetMethod
  }
};

var spiffs = { spiStart: 0x100000, spiEnd: 0x3FB000, spiPage: 256, spiBlock: 8192 };  // ESP-12E or F 3Mbyte spiffs
// var spiffs = { spiStart: 0x6B000, spiEnd: 0x7B000, spiPage: 256, spiBlock: 4096 };  // ESP-03 or -01 64Kbyte spiffs
// var spiffs = { spiStart: 0xDB000, spiEnd: 0xFB000, spiPage: 256, spiBlock: 4096 };  // ESP-07 128Kbyte spiffs

function arduinoBoardString (b) {
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

var xargs = [ '--board', arduinoBoardString(board), '--port',
  port, '-v', '--pref', 'build.path={PROJECT_PATH}/build', '{FILE_ACTIVE}' ];

function functionMatch (output) {
  const error = /^(..[^:]+):(\d+):\s+(\w+):(\s.*)$/;
  const error2 = /^(..[^:]+):(\d+):(\d+):\s+(\w+):(\s.*)$/;
  // this is the list of error matches that atom-build will process
  var array = [];
  // iterate over the output by lines
  output.split(/[\r\n]/).forEach(line => {
    // process possible error messages
    const errorMatch = error.exec(line);
    if (errorMatch) {
      // map the regex match to the error object that atom-build expects
      array.push({
        file: errorMatch[1].indexOf('.') > 1 ? errorMatch[1] : errorMatch[1] + '.ino',
        line: parseInt(errorMatch[2]),
        type: errorMatch[3] === 'note' ? 'Info' : errorMatch[3] === 'warning' ? 'Warning' : 'Error',
        message: errorMatch[4]
      });
    } else {
      const errorMatch2 = error2.exec(line);
      if (errorMatch2) {
        // map the regex match to the error object that atom-build expects
        array.push({
          file: errorMatch2[1].indexOf('.') > 1 ? errorMatch2[1] : errorMatch2[1] + '.ino',
          line: parseInt(errorMatch2[2]),
          col: parseInt(errorMatch2[3]),
          type: errorMatch2[4] === 'note' ? 'Info' : errorMatch2[4] === 'warning' ? 'Warning' : 'Error',
          message: errorMatch2[5]
        });
      }
    }
  });
  console.log(array);
  return array;
}

var terminalStart = function (buildOutcome, stdout, stderr) {
  if (!buildOutcome) return false;
  terminalProcess = require('child_process').spawn(
    'cmd',
    [ '/C', pathRunTerminal,
      'Port.PortName', port,
      'Port.BaudRate', terminalBaud
    ],
    { cwd: this.cwd, env: this.env }
  );
  return true;
};

var terminalKill = function () {
  if (terminalProcess) {
    terminalProcess.removeAllListeners();
    let tk = require(atom.packages.loadedPackages.build.path + '/node_modules/tree-kill');
    tk(terminalProcess.pid, 'SIGKILL');
    terminalProcess = null;
  }
};

module.exports = {
  cmd: pathRunArduino,
  args: [ '--verify' ].concat(xargs),
  name: 'compile',
  atomCommandName: 'build:compile',
  cwd: '{PROJECT_PATH}',
  sh: true,
  preBuild: terminalKill,
  functionMatch: functionMatch,
  targets: {
    upload: {
      atomCommandName: 'build:upload',
      cmd: pathRunArduino,
      args: [ '--upload' ].concat(xargs),
      preBuild: terminalKill,
      functionMatch: functionMatch
    },
    upload_then_terminal: {
      atomCommandName: 'build:upload_then_terminal',
      cmd: pathRunArduino,
      args: [ '--upload' ].concat(xargs),
      functionMatch: functionMatch,
      preBuild: terminalKill,
      postBuild: terminalStart
    },
    justupload: {
      atomCommandName: 'build:just_upload',
      cmd: pathRunEsptool,
      preBuild: terminalKill,
      args: [ '-vv', '-cd', resetMethod,
        '-cb', board.parameters.UploadSpeed,
        '-cp', port,
        '-ca', '0x00000',
        '-cf', '{PROJECT_PATH}/build/{FILE_ACTIVE_NAME}.bin' ]
    },
    spiffs:
    {
      atomCommandName: 'build:spiffs',
      cmd: 'echo Prep and Upload spiffs',
      preBuild: terminalKill,
      args: [
        '&&', 'xcopy data-uncompressed\\* data\\ /s /y',
        '&&', pathRunGzip, '-v', '-f',
        'data/*.html',
        'data/*.css',
        'data/*.js',
        'data/*-schema.json',
        '&&', pathRunMkspiffs,
        '-c', 'data',
        '-p', spiffs.spiPage,
        '-b', spiffs.spiBlock,
        '-s', (spiffs.spiEnd - spiffs.spiStart),
        'build/spiffs.bin',
        '&&', pathRunEsptool,
        '-vv',
        '-cd', resetMethod,
        '-cb', board.parameters.UploadSpeed,
        '-cp', port,
        '-ca', '0x' + spiffs.spiStart.toString(16),
        '-cf', '{PROJECT_PATH}/build/spiffs.bin'
      ]
    },
    terminal:
    {
      atomCommandName: 'build:terminal',
      preBuild: terminalKill,
      cmd: 'echo Running Terminal',
      postBuild: terminalStart
    },
    reset_then_terminal:
    {
      atomCommandName: 'build:reset_then_terminal',
      preBuild: terminalKill,
      cmd: pathRunEsptool,
      args: [ '-vv',
        '-cd', resetMethod,
        '-cb', board.parameters.UploadSpeed,
        '-cp', port,
        '-cr' ],
      postBuild: terminalStart
    },
    clean: {
      atomCommandName: 'clean',
      cmd: 'del {PROJECT_PATH}\\build /s/q && rmdir {PROJECT_PATH}\\build /s/q'
    }
  }
};
