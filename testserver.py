#
# ESPrinkler2 testserver.py
#
# This is a smiple server so data-uncompressed can be tested
# and edited using a local browser to http://localhost
# Note that data-uncompressed does not save config.json nor sched.json
# This server saves that data as strings
# Similarly current time and zone status are simulated
#
import SimpleHTTPServer
from urlparse import urlparse, parse_qs
import SocketServer
import os
import time
import json
import cgi

zones = [
        'off', 'off', 'off', 'off',
        'off', 'off', 'off', 'off'
        ]

htime = 0
ltime = time.time()
offsetGMT = 0
config = None
sched = None
buttons = None
host = 'ESPrinkler2'


class ESPrinkler2RequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
        def xsend(self, content, contentType="text/html"):
            print "serving custom response: " + content
            self.send_response(200)
            self.send_header("Content-Type", contentType)
            v = len(content)
            self.send_header("Content-Tength", v)
            self.end_headers()
            self.wfile.write(content)

        def do_GET(self):
            global zones, htime, ltime, config, sched, host, buttons, offsetGMT
            print self.path
            query_components = parse_qs(urlparse(self.path).query)
            self.path = self.path.split('?', 1)[0]
            print self.path
            if self.path == '/status':
                htime += time.time() - ltime
                ltime = time.time()
                v = {'time': int(htime), 'host': host, 'offsetGMT': offsetGMT}
                for i in range(0, 8):
                    v['zone'+str(i)] = zones[i]
                self.xsend(json.dumps(v), "text/json")
            elif self.path == '/settime':
                htime = 0
                try:
                    htime = int(query_components['time'][0])
                except:
                    pass
                ltime = time.time()
                try:
                    o = int(query_components['offset'][0])
                    offsetGMT = o
                except:
                    pass
                self.xsend("ok")
            elif self.path == '/clear':
                for i in range(0, 8):
                    zones[i] = 'off'
                self.xsend("ok")
            elif self.path == '/clean':
                config = None
                sched = None
                buttons = None
                self.xsend("Persistant Storage has been cleaned.")
            elif self.path == '/toggle':
                i = 0
                try:
                    i = int(query_components['zone'][0])
                except:
                    pass
                print i
                z = zones[i]
                if z == 'on':
                    z = 'off'
                else:
                    z = 'on'
                zones[i] = z
                self.xsend("ok")
            elif self.path == '/restart':
                self.xsend(
                    "Restarting... please wait a minute or two and refresh")
            elif self.path == '/list':
                v = []
                for f in os.listdir('.'):
                    v.append({'type': 'file', 'name': f})
                self.xsend(json.dumps(v), "text/json")
            # config.json is not saved, but only temp string storage
            elif self.path == '/config.json':
                if config is None:
                    self.send_error(404, "File not found")
                    return None
                else:
                    print('config sending\n'+config)
                    self.xsend(config)
            # sched.json is not saved, but only temp string storage
            elif self.path == '/sched.json':
                if sched is None:
                    self.send_error(404, "File not found")
                    return None
                else:
                    print('sched sending\n'+sched)
                    self.xsend(sched)
            # buttons.json is not saved, but only temp string storage
            elif self.path == '/buttons.json':
                if buttons is None:
                    self.send_error(404, "File not found")
                    return None
                else:
                    print('buttons sending\n'+buttons)
                    self.xsend(buttons)
            else:
                return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

        def do_DELETE(self):
            self.path = self.path.split('?', 1)[0]
            print self.path
            if self.path == '/edit':
                form = cgi.FieldStorage(
                    fp=self.rfile,
                    headers=self.headers,
                    environ={'REQUEST_METHOD': 'POST',
                             'CONTENT_TYPE': self.headers['Content-Type'],
                             })

                for field in form.keys():
                    field_item = form[field]
                    if field == 'path':
                        fn = field_item.value
                        if fn.startswith('/'):
                            fn = fn[1:]
                        if os.path.isfile(fn):
                            os.remove(fn)
                            self.xsend("")
                            return

            self.send_response(404)

        def do_PUT(self):
            self.path = self.path.split('?', 1)[0]
            print self.path
            if self.path == '/edit':
                form = cgi.FieldStorage(
                    fp=self.rfile,
                    headers=self.headers,
                    environ={'REQUEST_METHOD': 'POST',
                             'CONTENT_TYPE': self.headers['Content-Type'],
                             })

                for field in form.keys():
                    field_item = form[field]
                    if field == 'path':
                        fn = field_item.value
                        if fn.startswith('/'):
                            fn = fn[1:]
                        if not os.path.isfile(fn):
                            f = open(fn, 'w')
                            f.close()
                            self.xsend("")
                            return
                        else:
                            self.send_response(404)
                            self.end_headers()
                            return

            self.send_response(404)

        def do_POST(self):
            global config, sched, buttons, host, offsetGMT
            self.path = self.path.split('?', 1)[0]
            print self.path
            if self.path == '/edit':

                form = cgi.FieldStorage(
                    fp=self.rfile,
                    headers=self.headers,
                    environ={'REQUEST_METHOD': 'POST',
                             'CONTENT_TYPE': self.headers['Content-Type'],
                             })

                for field in form.keys():
                    field_item = form[field]
                    if field_item.filename:
                        # The field contains an uploaded file
                        file_data = field_item.file.read()
                        file_len = len(file_data)
                        print('Uploaded %s as "%s" (%d bytes)' %
                              (field, field_item.filename, file_len))
                        if field == 'data':
                            fn = field_item.filename
                            if fn.startswith('/'):
                                fn = fn[1:]
                            f = open(fn, 'w')
                            f.write(file_data)
                            f.close()
                            print 'wrote:'+fn
                            # config and sched are never saved for testing
                            if (fn == 'config.json' or fn == 'sched.json'
                               or fn == 'buttons.json'):
                                os.remove(fn)
                            if fn == 'config.json':
                                config = file_data
                                try:
                                    tt = json.loads(file_data)
                                    host = tt['host']
                                    x = int(tt['offsetGMT'])
                                    if x != -1:
                                        x = offsetGMT
                                except:
                                    pass
                                print('config\n'+file_data)
                            if fn == 'sched.json':
                                sched = file_data
                                print('sched\n'+file_data)
                            if fn == 'buttons.json':
                                buttons = file_data
                                print('buttons\n'+file_data)
                        del file_data
                    else:
                        # Regular form value
                        print('%s=%s' % (field, form[field].value))

                self.xsend("<html><body>ok</body></html>")
            else:
                return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


os.chdir("data-uncompressed")
PORT = 80

Handler = ESPrinkler2RequestHandler

httpd = SocketServer.TCPServer(("", PORT), Handler)

print "serving at port", PORT
print "serving folder", os.getcwd()
httpd.serve_forever()
