use core::panic;
use std::str::FromStr;

use actix_web::web;
use actix_web::HttpServer;
use log::debug;
use sqlx::sqlite;

mod commons;
mod queries;
mod views;

#[actix_web::main]
async fn main() -> anyhow::Result<()> {
    println!("Starting server...");

    dotenvy::dotenv().unwrap_or_else(|_| panic!("Cannot read env file"));
    env_logger::init();

    let opts = sqlite::SqliteConnectOptions::from_str(&dotenvy::var("DATABASE_URL")?)?
        .journal_mode(sqlite::SqliteJournalMode::Wal)
        .foreign_keys(true)
        .extension_with_entrypoint("./target/ext/uuid.dll", "uuid_init")
        .extension_with_entrypoint("./target/ext/fuzzy.dll", "fuzzy_init")
        .extension("./target/ext/csv.dll");

    let pool = sqlite::SqlitePool::connect_with(opts)
        .await
        .unwrap_or_else(|_| panic!("Cannot establish db connection"));

    sqlx::migrate!("./migrations")
        .run(&pool)
        .await
        .unwrap_or_else(|e| {
            debug!("{:?}", e);
            panic!("Database migration failed!")
        });

    let mut env = minijinja::Environment::new();
    env.set_loader(minijinja::path_loader("./templates"));

    HttpServer::new(move || {
        let note_services = web::scope("/notes")
            .service(views::notes::new_note)
            .service(views::notes::list_notes)
            .service(views::notes::detail_note)
            .service(views::notes::update_note_body)
            .service(views::notes::search_note);

        let file_services = web::scope("/fs")
            .service(views::fs::file_explorer)
            .service(views::fs::file_search)
            .service(views::fs::all_files);

        actix_web::App::new()
            .app_data(web::Data::new(pool.clone()))
            .app_data(web::Data::new(env.clone()))
            .service(note_services)
            .service(views::home::home_page)
            .service(file_services)
    })
    .bind(("localhost", 8080))?
    .run()
    .await?;

    Ok(())
}
