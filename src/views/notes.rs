use super::utils::{HtmlResponse, InternalServerError};
use actix_web::web;
use log::debug;
use sqlx::sqlite::SqlitePool;

#[derive(sqlx::FromRow, serde::Serialize)]
struct LiNote {
    url: String,
    name: String,
}

#[actix_web::get("/")]
pub async fn list_notes(
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("list_notes.html").unwrap();
    let notes = sqlx::query_as::<_, LiNote>(
        "
        SELECT concat('/notes/', uid) as url, filename as name
        FROM note;
    ",
    )
    .fetch_all(db.get_ref())
    .await
    .unwrap();

    HtmlResponse().body(
        tmpl.render(minijinja::context! {
            notes => minijinja::Value::from_serializable(&notes)
        })
        .unwrap(),
    )
}

#[derive(sqlx::FromRow, serde::Serialize)]
struct DisplayNote {
    filename: String,
    body: String,
}

#[actix_web::get("/{uid}")]
pub async fn detail_note(
    path: web::Path<String>,
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("detail_note.html").unwrap();
    let uid = path.into_inner();
    let note = sqlx::query_as::<_, DisplayNote>(
        "
        SELECT filename, body 
        FROM note
        WHERE uid = ?;
    ",
    )
    .bind(uid)
    .fetch_one(db.get_ref())
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
struct UpdateBody {
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
    match sqlx::query(
        "
        UPDATE note 
        SET body = $1
        WHERE uid = $2;
    ",
    )
    .bind(&form.body)
    .bind(uid)
    .execute(db.get_ref())
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
struct NewNoteRequest {
    filename: String,
}

#[actix_web::post("/")]
pub async fn new_note(
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
    form: web::Form<NewNoteRequest>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("fragments/notification.html").unwrap();

    match sqlx::query(
        "
        INSERT INTO note(filename)
        VALUES (?);
        ",
    )
    .bind(&form.filename)
    .execute(db.get_ref())
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
            message => "Successfully created"
        })
        .unwrap(),
    )
}
