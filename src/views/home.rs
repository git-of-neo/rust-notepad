use super::utils::HtmlResponse;
use actix_web::web;

#[actix_web::get("/")]
pub async fn home_page(env: web::Data<minijinja::Environment<'_>>) -> impl actix_web::Responder {
    let tmpl = env.get_template("home.html").unwrap();

    self::HtmlResponse().body(tmpl.render(minijinja::context! {}).unwrap())
}
