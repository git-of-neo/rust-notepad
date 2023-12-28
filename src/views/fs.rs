use super::utils::HtmlResponse;
use crate::queries::Queries;
use actix_web::web;
use sqlx::sqlite::SqlitePool;

#[derive(sqlx::FromRow, serde::Serialize)]
struct DisplayFile {
    url: String,
    display_filename: String,
}

impl<'a> Queries<'a> {
    async fn get_all_files(&self) -> Result<Vec<DisplayFile>, sqlx::Error> {
        sqlx::query_as::<_, DisplayFile>(
            "
            SELECT concat('/notes/', uid) as url, display_filename 
            FROM file_tree LIMIT 20;
        ",
        )
        .fetch_all(self.pool)
        .await
    }

    async fn search_files<'tmp>(
        &self,
        fuzzy_name: &'tmp str,
    ) -> Result<Vec<DisplayFile>, sqlx::Error> {
        sqlx::query_as::<_, DisplayFile>(
            "
            SELECT concat('/notes/', uid) as url, display_filename 
            FROM file_tree 
            ORDER BY levenshtein(display_filename, ?)
            LIMIT 20;
        ",
        )
        .bind(fuzzy_name)
        .fetch_all(self.pool)
        .await
    }
}

#[actix_web::get("/")]
pub async fn file_explorer(
    env: web::Data<minijinja::Environment<'_>>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("fs/index.jinja").unwrap();
    HtmlResponse().body(tmpl.render(minijinja::context! {}).unwrap())
}

#[actix_web::get("/all")]
pub async fn all_files(
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("fs/search-results.jinja").unwrap();
    let files = Queries::new(db.get_ref()).get_all_files().await.unwrap();

    HtmlResponse().body(
        tmpl.render(minijinja::context! {
            files => minijinja::Value::from_serializable(&files)
        })
        .unwrap(),
    )
}

#[derive(serde::Deserialize)]
struct SearchRequest {
    query: String,
}

#[actix_web::post("/search")]
pub async fn file_search(
    env: web::Data<minijinja::Environment<'_>>,
    db: web::Data<SqlitePool>,
    form: web::Form<SearchRequest>,
) -> impl actix_web::Responder {
    let tmpl = env.get_template("fs/search-results.jinja").unwrap();
    let files = Queries::new(db.get_ref())
        .search_files(&form.query)
        .await
        .unwrap();
    HtmlResponse().body(
        tmpl.render(minijinja::context! {
            files => minijinja::Value::from_serializable(&files)
        })
        .unwrap(),
    )
}
