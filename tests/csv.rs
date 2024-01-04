use sqlx::{sqlite, Connection};
use std::str::FromStr;

#[derive(sqlx::FromRow, PartialEq, Debug)]
struct Row {
    name: String,
    age: i64,
    city: String,
    occupation: String,
}

#[sqlx::test]
async fn it_works() {
    let opts = sqlite::SqliteConnectOptions::from_str("sqlite::memory:")
        .unwrap()
        .extension("./target/ext/csv.dll");

    let mut conn = sqlite::SqliteConnection::connect_with(&opts).await.unwrap();
    sqlx::query(
        "
        CREATE VIRTUAL TABLE temp.x USING csv(./tests/data/worker.csv);
    ",
    )
    .execute(&mut conn)
    .await
    .unwrap_or_else(|_| panic!("Failed to create table"));

    let rows = sqlx::query_as::<_, Row>(
        "
        SELECT Name as name, CAST(Age as INTEGER) as age, City as city, Occupation as occupation
        FROM temp.x;
    ",
    )
    .fetch_all(&mut conn)
    .await
    .unwrap();

    assert_eq!(
        rows,
        vec![
            Row {
                name: "John".to_owned(),
                age: 25,
                city: "New York".to_owned(),
                occupation: "Engineer".to_owned(),
            },
            Row {
                name: "Emily".to_owned(),
                age: 30,
                city: "Los Angeles".to_owned(),
                occupation: "Teacher".to_owned(),
            },
            Row {
                name: "Michael".to_owned(),
                age: 22,
                city: "Chicago".to_owned(),
                occupation: "Student".to_owned(),
            },
            Row {
                name: "Sophia".to_owned(),
                age: 35,
                city: "San Francisco".to_owned(),
                occupation: "Doctor".to_owned(),
            },
            Row {
                name: "Daniel".to_owned(),
                age: 28,
                city: "Miami".to_owned(),
                occupation: "Artist".to_owned(),
            },
        ]
    );
}
