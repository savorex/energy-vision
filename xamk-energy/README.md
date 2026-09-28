# XAMK Energy App
Metering services and demonstrability for clients for XAMK campuses.

## Project Strucure:
```
project/
    docker-compose.yml
    db/
        init.sql
    app/
        Dockerfile
        package.json
        server.js
        public/
            index.html
```

## Building & Running container:
Docker-Compose manages both building and running the application.
```sudo docker-compose up -d --build```

If you already started the DB once, changing init.sql will do nothing unless you remove the volume:
```
sudo docker-compose down -v
sudo docker-compose up
```
Warning: this deletes database data.

For further reading, checkout:
```docker-compose.yml```