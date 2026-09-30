# Build
FROM node:22-alpine AS builder
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm install
COPY . .

# Serve
FROM nginx:1.25-alpine
COPY --from=builder /app/src /usr/share/nginx/html
COPY ./nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 6767
CMD ["nginx", "-g", "daemon off;"]
