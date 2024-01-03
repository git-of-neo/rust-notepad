use super::utils::{HtmlResponse, InternalServerError};
use crate::commons::constants;
use crate::queries::Queries;
use actix_web::web;
use log::debug;
use sqlx::sqlite::SqlitePool;
use html_to_string_macro::html;


#[derive(sqlx::FromRow, serde::Serialize)]
struct LiNote {
    url: String,
    name: String,
}

#[derive(sqlx::FromRow, serde::Serialize)]
struct DisplayNote {
    filename: String,
    body: String,
}


impl<'a> Queries<'a> {
    async fn get_all_notes(&self) -> Result<Vec<LiNote>, sqlx::Error> {
        sqlx::query_as::<_, LiNote>(
            "
            SELECT concat('/notes/', uid) as url, display_filename as name
            FROM file_tree;
        ",
        )
        .fetch_all(self.pool)
        .await
    }

    async fn get_note_by_uid(&self, uid: String) -> Result<DisplayNote, sqlx::Error> {
        sqlx::query_as::<_, DisplayNote>(
            "
            SELECT display_filename as filename, CAST(data AS TEXT) as body 
            FROM file_tree a JOIN data_block b
            ON a.inode = b.inode
            WHERE a.uid = ?;
        ",
        )
        .bind(uid)
        .fetch_one(self.pool)
        .await
    }

    async fn update_note_body<'tmp>(
        &self,
        uid: &'tmp str,
        body: &'tmp str,
    ) -> Result<(), sqlx::Error> {
        match sqlx::query(
            "
            UPDATE data_block
            SET data = ?
            WHERE inode = (SELECT inode FROM file_tree WHERE uid = ?);
        ",
        )
        .bind(&body.as_bytes())
        .bind(&uid)
        .execute(self.pool)
        .await
        {
            Ok(_) => Ok(()),
            Err(e) => Err(e),
        }
    }

    async fn new_note<'tmp>(&self, filename: &'tmp str) -> Result<(), sqlx::Error> {
        self.new_file(constants::PLAIN_TEXT, None, filename, "".as_bytes())
            .await
    }

    async fn search_note<'tmp>(&self, t: &'tmp str) -> Result<Vec<LiNote>, sqlx::Error>{
        sqlx::query_as::<_, LiNote>(
            "
            SELECT concat('/notes/', uid) as url, display_filename as name
            FROM file_tree
            ORDER BY levenshtein(display_filename, ?);
        ",
        )
        .bind(t)
        .fetch_all(self.pool)
        .await
    }
}

#[actix_web::get("/")]
pub async fn list_notes(
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("list_notes.html").unwrap();
    let notes = Queries::new(db.get_ref()).get_all_notes().await.unwrap();

    HtmlResponse().body(
        tmpl.render(minijinja::context! {
            notes => minijinja::Value::from_serializable(&notes)
        })
        .unwrap(),
    )
}

#[actix_web::get("/{uid}")]
pub async fn detail_note(
    path: web::Path<String>,
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("detail_note.html").unwrap();
    let uid = path.into_inner();
    let note = Queries::new(db.get_ref())
        .get_note_by_uid(uid)
        .await
        .unwrap();

    HtmlResponse().body(
        tmpl.render(minijinja::context! {
            note => minijinja::Value::from_serializable(&note)
        })
        .unwrap(),
    )
}

#[derive(serde::Deserialize)]
pub struct UpdateBody {
    body: String,
}

#[actix_web::patch("/{uid}")]
pub async fn update_note_body(
    path: web::Path<String>,
    form: web::Form<UpdateBody>,
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("fragments/notification.html").unwrap();
    let uid = path.into_inner();

    match Queries::new(db.get_ref())
        .update_note_body(&uid, &form.body)
        .await
    {
        Ok(_) => (),
        Err(e) => {
            debug!("{:?}", e);
            return InternalServerError().body(
                tmpl.render(minijinja::context! {
                    message => "Failed to save!"
                })
                .unwrap(),
            );
        }
    }

    HtmlResponse().body(
        tmpl.render(minijinja::context! {
            message => "saved"
        })
        .unwrap(),
    )
}

#[derive(serde::Deserialize)]
pub struct NewNoteRequest {
    filename: String,
}

#[actix_web::post("/")]
pub async fn new_note(
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
    form: web::Form<NewNoteRequest>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("fragments/notification.html").unwrap();

    match Queries::new(db.get_ref()).new_note(&form.filename).await {
        Ok(_) => (),
        Err(e) => {
            debug!("{:?}", e);
            return InternalServerError().body(
                tmpl.render(minijinja::context! {
                    message => "Failed to save!"
                })
                .unwrap(),
            );
        }
    }

    HtmlResponse().body(
        tmpl.render(minijinja::context! {
            message => "Successfully created"
        })
        .unwrap(),
    )
}


#[derive(serde::Deserialize)]
struct SearchNoteRequest {
    search: String,
}

#[actix_web::post("/search")]
pub async fn search_note(
    // env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
    form: web::Form<SearchNoteRequest>,
) -> impl actix_web::Responder{
    // let tmpl = env.get_template("list_notes.html").unwrap();
    let notes = Queries::new(db.get_ref()).search_note(&form.search).await.unwrap();

    let mut body = String::new();

    for linote in notes.iter(){
        body.push_str(&html!(
            <tr><td>{&linote.url}</td><td>{&linote.name}</td></tr>
        ));
    }

    HtmlResponse().body(body)
}