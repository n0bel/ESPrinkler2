
var path_to_arduino = '..\\..\\..\\ides\\arduino-1.6.11\\arduino_debug.exe';
var path_to_mkspiffs = '..\\..\\..\\ides\\arduino-1.6.11\\portable\\packages\\esp8266\\tools\\mkspiffs\\0.1.2\\mkspiffs.exe';
var path_to_esptool = '..\\..\\..\\ides\\arduino-1.6.11\\portable\\packages\\esp8266\\tools\\esptool\\0.4.9\\esptool.exe';
var path_to_terminal = '..\\..\\..\\terminal\\termite.exe';

var port = 'COM4';
var board = {
  package: 'esp8266',
  arch: 'esp8266',
  board: 'generic',
  parameters: {
    UploadSpeed: 256000,
    CpuFrequency: 80,
    Debug: 'Disabled',
    DebugLevel: 'None____',
    FlashFreq: 40,
    FlashMode: 'dio',
    FlashSize: '4M3M',  //ESP-12E or F   4Mbyte
    //FlashSize: '512K64',  //ESP-03 or -01   512Kbyte
    //FlashSize: '1M128',  //ESP-07    1MByte
    ResetMethod: 'nodemcu'
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

module.exports = {
  cmd: path_to_arduino,
  args: [ "--verify", "--board", arduinoBoardString(board), '-v', '--pref', 'build.path={PROJECT_PATH}/build', '{FILE_ACTIVE}' ],
  name: 'compile',
  cwd: '{PROJECT_PATH}',
  sh: true,
  targets: {
    upload: {
      cmd: path_to_arduino,
      args: [ "--upload", "--board", arduinoBoardString(board) , "--port", port, '-v', '--pref', 'build.path={PROJECT_PATH}/build', '{FILE_ACTIVE}' ]
    },
    upload: {
      cmd: path_to_arduino,
      args: [ "--upload", "--board", arduinoBoardString(board) , "--port", port, '-v', '--pref', 'build.path={PROJECT_PATH}/build', '{FILE_ACTIVE}' ]
    },
    justupload: {
      cmd: path_to_esptool,
      args: [ '-vv', "-cd", board.parameters.ResetMethod, "-cb", board.parameters.UploadSpeed, "-cp", port, '-ca', '0x00000', '-cf', "{PROJECT_PATH}/build/{FILE_ACTIVE_NAME}.bin" ]
    },
    spiffs:
    {
      cmd: path_to_mkspiffs,
      args: [ '-c', '{PROJECT_PATH}/data', '-p', spiffs.spiPage, '-b', spiffs.spiBlock, '-s',  (spiffs.spiEnd - spiffs.spiStart), '{PROJECT_PATH}/build/spiffs.bin',
        '&&', path_to_esptool, '-vv', "-cd", board.parameters.ResetMethod, "-cb", board.parameters.UploadSpeed, "-cp", port, "-ca", '0x'+spiffs.spiStart.toString(16), "-cf", '{PROJECT_PATH}/build/spiffs.bin' ]
    },
    terminal:
    {
      cmd: path_to_terminal
    },
    clean: {
      cmd: 'del {PROJECT_PATH}\\build /s/q && rmdir {PROJECT_PATH}\\build /s/q'
    }
  },
  preBuild: function()
  {
    return true;
  },
  postBuild: function()
  {
    return true;
  },
  functionMatch: function (output) {
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
};
