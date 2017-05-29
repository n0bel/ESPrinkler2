import SimpleHTTPServer
import SocketServer
import os
import time
import json
import cgi

s1 = 'unlatched'
s2 = 'unlatched'

class EspFeederRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
        def xsend(self,content,contentType="text/html"):
            print "serving custom response: " + content
            self.send_response(200)
            self.send_header("Content-Type", contentType)
            v = len(content)
            self.send_header("Content-Tength", v)
            self.end_headers()
            self.wfile.write(content)

        def do_GET(self):
            global s1,s2
            self.path = self.path.split('?',1)[0]
            print self.path
            if self.path == '/status':
                v = { 's1': s1, 's2': s2, 'time': int(time.time()) };
                self.xsend(json.dumps(v),"text/json")
            elif self.path == '/toggle1':
                if s1 == 'unlatched': s1 = 'latched'
                else: s1 = 'unlatched'
                self.xsend("ok")
            elif self.path == '/toggle2':
                if s2 == 'unlatched': s2 = 'latched'
                else: s2 = 'unlatched'
                self.xsend("ok")
            elif self.path == '/restart':
                self.xsend("Restarting... please wait a minute or two and refresh")
            elif self.path == '/list':
                v = []
                for f in os.listdir('.'):
                    v.append( { 'type': 'file', 'name': f })
                self.xsend(json.dumps(v),"text/json")
            else:
                return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

        def do_DELETE(self):
            self.path = self.path.split('?',1)[0]
            print self.path
            if self.path == '/edit':
                form = cgi.FieldStorage(
                fp=self.rfile,
                headers=self.headers,
                environ={'REQUEST_METHOD':'POST',
                         'CONTENT_TYPE':self.headers['Content-Type'],
                         })

                for field in form.keys():
                    field_item = form[field]
                    if field == 'path':
                        fn = form[field].value
                        if fn.startswith('/'): fn = fn[1:]
                        if os.path.isfile(fn):
                            os.remove(fn)
                            self.xsend("")
                            return

            self.send_response(404)

        def do_PUT(self):
            self.path = self.path.split('?',1)[0]
            print self.path
            if self.path == '/edit':
                form = cgi.FieldStorage(
                fp=self.rfile,
                headers=self.headers,
                environ={'REQUEST_METHOD':'POST',
                         'CONTENT_TYPE':self.headers['Content-Type'],
                         })

                for field in form.keys():
                    field_item = form[field]
                    if field == 'path':
                        fn = form[field].value
                        if fn.startswith('/'): fn = fn[1:]
                        if not os.path.isfile(fn):
                            f = open(fn,'w')
                            f.close()
                            self.xsend("")
                            return
                        else:
                            self.send_response(404)
                            self.end_headers()
                            return

            self.send_response(404)


        def do_POST(self):
            self.path = self.path.split('?',1)[0]
            print self.path
            if self.path == '/edit':

                form = cgi.FieldStorage(
                fp=self.rfile,
                headers=self.headers,
                environ={'REQUEST_METHOD':'POST',
                         'CONTENT_TYPE':self.headers['Content-Type'],
                         })

                for field in form.keys():
                    field_item = form[field]
                    if field_item.filename:
                        # The field contains an uploaded file
                        file_data = field_item.file.read()
                        file_len = len(file_data)
                        print('Uploaded %s as "%s" (%d bytes)' % \
                                (field, field_item.filename, file_len))
                        if field == 'data':
                            fn = field_item.filename
                            if fn.startswith('/'): fn = fn[1:]
                            f = open(fn,'w')
                            f.write(file_data)
                            f.close()
                            print 'wrote:'+fn
                        del file_data
                    else:
                        # Regular form value
                        print('%s=%s' % (field, form[field].value))

                self.xsend("<html><body>ok</body></html>")
            else:
                return SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)


#os.chdir("data-uncompressed")
PORT = 80

Handler = EspFeederRequestHandler

httpd = SocketServer.TCPServer(("", PORT), Handler)

print "serving at port", PORT
httpd.serve_forever()
