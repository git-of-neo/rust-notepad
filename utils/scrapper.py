# Gets a csv of python source file names from github for testing
import requests
import csv

url = "https://api.github.com/repos/python/cpython/git/trees/main?recursive=1"

r = requests.get(url)
tree = r.json()["tree"]
with open("files.csv", "w", newline="") as f:
    writer = csv.writer(f)    
    writer.writerow(("rowid", "filepath"))
    for idx, file in enumerate(tree):
        writer.writerow((idx, file["path"]))

    