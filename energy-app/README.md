# Energy App
Web-based demonstrability for clients via metering services.

## Project Strucure:
```
energy-app/
    app/
        Dockerfile
        package.json
        server.js
        src/
            index.html
    api/
        Dockerfile
        package.json
        server.js
    db/
        init.sql
    docker-compose.yml
```

You need Docker Compose (V2) to run the commands: install it via ```sudo apt install docker-compose-v2```

## Building & Running container:
Docker Compose manages both building and running the application.

`sudo docker compose -p energy-vision up --build -d`

Note: you can set the name of the application yourself. Change `energy-vision` to your liking.

## Setting up Nginx proxy:
### 1. Copy the site config
`sudo cp app/nginx.conf /etc/nginx/sites-available/energy-vision`

### 2. Enable it = symlink into sites-enabled
`sudo ln -s /etc/nginx/sites-available/energy-vision /etc/nginx/sites-enabled/`

### 3. Validate, then reload
`sudo nginx -t`
`sudo systemctl reload nginx`

### To disable a site (file stays in sites-available)
`sudo rm /etc/nginx/sites-enabled/energy-vision`
`sudo systemctl reload nginx`

### one-off: apply the new table to the running DB
`sudo docker exec -i energy-vision-db mysql -usankey -psankeypass sankeydb < db/init/02-chart-info.sql`


If you already started the DB once, changing init.sql will do nothing unless you remove the volume:
```
sudo docker compose down -v
sudo docker compose up
```
Warning: this deletes database data.


### Force Docker to stop and remove ALL CONTAINERS. DO NOT RUN!
```
sudo docker stop $(sudo docker ps -a -q)
sudo docker rm $(sudo docker ps -a -q)
```