from fastapi import FastAPI, Response
import sqlite3
import struct

OK_CODE = 0
ENOENT_CODE = 2
EEXIST_CODE = 17
ENOTEMPTY_CODE = 39

MAX_FILE_SIZE       = 1024
MAX_DENTRY_NAME_LEN = 128
ROOT_INO            = 1000

DB_FILE = 'filesystem.db'

next_ino = ROOT_INO


# SQL FUNCTIONS

def make_response(data):
    return Response(status_code=200, content=data)

def get_cursor():
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    return (conn, conn.cursor())

def get_file_by_name(name, parent_inode):
    conn, c = get_cursor()
    with conn:
        c.execute('SELECT * FROM files WHERE name=? AND parent_inode=?', (name, parent_inode))
        file = c.fetchone()
    return file

def get_files_by_parent_inode(parent_inode):
    conn, c = get_cursor()
    with conn:
        c.execute('SELECT * FROM files WHERE parent_inode=?', (parent_inode,))
        files = c.fetchall()
    return files

def create_file(name, parent_inode, mode, data=None):
    global next_ino
    conn, c = get_cursor()
    with conn:
        next_ino += 1
        c.execute('INSERT INTO files (inode, name, parent_inode, mode, data) VALUES (?, ?, ?, ?, ?)', (next_ino, name, parent_inode, mode, data))

def delete_file(inode):
    conn, c = get_cursor()
    with conn:
        c.execute('DELETE FROM files WHERE inode=?', (inode,))
        ret = c.rowcount
    return ret

def get_data_by_inode(inode):
    conn, c = get_cursor()
    with conn:
        c.execute('SELECT data FROM files WHERE inode=?', (inode,))
        data = c.fetchone()
    return data

def set_data_by_inode(inode, data):
    conn, c = get_cursor()
    with conn:
        c.execute('UPDATE files SET data=? WHERE inode=?', (data, inode))
        ret = c.rowcount
    return ret

def get_max_inode():
    conn, c = get_cursor()
    with conn:
        c.execute("SELECT MAX(inode) FROM files")
        max_inode = c.fetchone()[0]
    return max_inode

def init_db():
    conn, c = get_cursor()
    with conn:
        c.execute(f'''
        CREATE TABLE IF NOT EXISTS files(
        inode INTEGER PRIMARY KEY,
        name TEXT({MAX_DENTRY_NAME_LEN}) NOT NULL,
        parent_inode INTEGER,mode INTEGER NOT NULL,
        data TEXT({MAX_FILE_SIZE}) DEFAULT NULL,
        UNIQUE(name, parent_inode)
        )
        ''')


# INIT

app = FastAPI()
init_db()


# INODE OPERATIONS

@app.get('/api/lookup')
def lookup(name: str, parent_inode: int, token: str='default'):
    file = get_file_by_name(name, parent_inode)
    if file:
        packed_data = struct.pack('<q', OK_CODE)
        packed_data += struct.pack('<I', file['inode'])
        packed_data += struct.pack('<I', file['mode'])
    else:
        packed_data = struct.pack('<q', ENOENT_CODE)
    return make_response(packed_data)

@app.get('/api/create')
def create(name: str, parent_inode: int, mode: int, token: str='default'):
    try:
        create_file(name, parent_inode, mode)
        file = get_file_by_name(name, parent_inode)
    except sqlite3.IntegrityError as e:
        packed_data = struct.pack('<q', EEXIST_CODE)
        return make_response(packed_data)
    packed_data = struct.pack('<q', OK_CODE)
    packed_data += struct.pack('<I', file['inode'])
    return make_response(packed_data)

@app.get('/api/remove')
def remove(name: str, parent_inode: int, token: str='default'):
    file = get_file_by_name(name, parent_inode)
    if not file:
        packed_data = struct.pack('<q', ENOENT_CODE)
    elif get_files_by_parent_inode(file['inode']):
        packed_data = struct.pack('<q', ENOTEMPTY_CODE)
    else:
        delete_file(file['inode'])
        packed_data = struct.pack('<q', OK_CODE)
    return make_response(packed_data)


# FILE OPERATIONS

@app.get('/api/iterate')
def iterate(parent_inode: int, token: str='default'):
    files = get_files_by_parent_inode(parent_inode)
    packed_data = struct.pack('<q', OK_CODE)
    packed_data += struct.pack('<i', len(files))
    for file in files:
        packed_data += struct.pack(f'<{MAX_DENTRY_NAME_LEN}s', file['name'].encode().ljust(MAX_DENTRY_NAME_LEN, b'\x00'))
        packed_data += struct.pack('<I', file['inode'])
        packed_data += struct.pack('<I', file['mode'])
    return make_response(packed_data)

@app.get('/api/read')
def read(inode: int, token: str='default'):
    result = get_data_by_inode(inode)
    if (result):
        packed_data = struct.pack('<q', OK_CODE)
        packed_data += struct.pack('<L', len(result['data']))
        packed_data += struct.pack(f'<{len(result["data"])}s', result['data'].encode())
    else:
        packed_data = struct.pack('<q', ENOENT_CODE)
    return make_response(packed_data)

@app.get('/api/write')
def write(inode: int, data: str, offset: int, token: str='default'):
    print(data)
    print(offset)
    if offset != 0:
        prev_data = get_data_by_inode(inode)['data']
        data = prev_data[:offset] + data + prev_data[offset + len(data):]
    ret = set_data_by_inode(inode, data)
    packed_data = struct.pack('<q', OK_CODE)
    return make_response(packed_data)
