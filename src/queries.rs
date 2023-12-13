use log::debug;
use sqlx::sqlite::SqlitePool;

type ID = i64;

const FILE_FLAG: i8 = 1;
const DIRECTORY_FLAG: i8 = 2;

pub struct Queries<'a> {
    pub pool: &'a SqlitePool,
}

impl<'a> Queries<'a> {
    pub fn new(executor: &'a SqlitePool) -> Self {
        Self { pool: executor }
    }

    pub async fn new_file<'tmp>(
        &self,
        mime_type: &'tmp str,
        parent: Option<ID>,
        filename: &'tmp str,
        data: &'tmp [u8],
    ) -> Result<(), sqlx::Error> {
        let conn = self.pool;
        let trx = conn.begin().await?;

        let inode: ID;
        match sqlx::query_scalar(
            "
                INSERT INTO file_tree(
                    parent, display_filename
                )
                VALUES (
                    ?, ?
                ) 
                RETURNING inode;
            ",
        )
        .bind(parent)
        .bind(filename)
        .fetch_one(self.pool)
        .await
        {
            Ok(id) => inode = id,
            Err(e) => {
                debug!("{:?}", e);
                trx.rollback().await?;
                return Err(e);
            }
        }

        match sqlx::query(
            "
                INSERT INTO inode(
                    inode, file_type
                )
                VALUES (
                    ?, ?
                )
            ",
        )
        .bind(inode)
        .bind(FILE_FLAG)
        .execute(self.pool)
        .await
        {
            Ok(_) => (),
            Err(e) => {
                debug!("{:?}", e);
                trx.rollback().await?;
                return Err(e);
            }
        }

        match sqlx::query(
            "
                INSERT INTO data_block(
                    inode, mime_type, data
                )
                VALUES (
                    ?, ?, ?
                )
            ",
        )
        .bind(inode)
        .bind(mime_type)
        .bind(data)
        .execute(self.pool)
        .await
        {
            Ok(_) => (),
            Err(e) => {
                debug!("{:?}", e);
                trx.rollback().await?;
                return Err(e);
            }
        }

        trx.commit().await?;
        Ok(())
    }
}
