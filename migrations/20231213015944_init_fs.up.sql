-- Add up migration script here
CREATE TABLE file_tree(
    inode INTEGER PRIMARY KEY,
    parent INTEGER,
    uid TEXT,
    display_filename TEXT NOT NULL,
    FOREIGN KEY(parent) REFERENCES file_tree(inode)
);

CREATE TRIGGER file_tree_set_uid
AFTER INSERT ON file_tree
BEGIN
    UPDATE file_tree
    SET uid = uuid1()
    WHERE inode = NEW.inode;
END;

CREATE TABLE inode(
    id INTEGER PRIMARY KEY,
    inode INTEGER,
    file_type INTEGER,
    FOREIGN KEY(inode) REFERENCES file_tree(inode)
);

CREATE TABLE data_block(
    id INTEGER PRIMARY KEY,
    inode INTEGER,
    mime_type TEXT NOT NULL,
    data BLOB,
    FOREIGN KEY(inode) REFERENCES   file_tree(inode)
);

