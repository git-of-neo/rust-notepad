#[allow(non_snake_case)]
pub fn HtmlResponse() -> actix_web::HttpResponseBuilder {
    let mut resp = actix_web::HttpResponse::Ok();
    resp.content_type("text/html; charset=utf-8");
    resp
}

#[allow(non_snake_case)]
pub fn InternalServerError() -> actix_web::HttpResponseBuilder {
    let mut resp = actix_web::HttpResponse::Ok();
    resp.content_type("text/html; charset=utf-8");
    resp
}
