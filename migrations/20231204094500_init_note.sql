-- Add migration script here
CREATE TABLE note (
    id INTEGER PRIMARY KEY,
    filename TEXT NOT NULL,
    uid INTEGER NOT NULL UNIQUE, 
    body TEXT NOT NULL DEFAULT ''
);

CREATE TRIGGER set_uid
AFTER INSERT ON note
BEGIN
    UPDATE note
    SET uid = uuid1()
    WHERE id = NEW.id;
END;
