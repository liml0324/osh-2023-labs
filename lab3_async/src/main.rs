use std::{
    //io::{prelude::*, BufReader},
    fs,
    env,//fmt::{format, Debug},
};

use async_std::{
    net::{TcpListener, TcpStream},
    prelude::*,
};

use futures::stream::StreamExt;

#[async_std::main]
async fn main() {
    let args:Vec<String> = env::args().collect();
    let mut debug = false;
    let mut port = "8000";

    for i in 1..args.len() {
        if args[i] == "-d" {
            println!("Debug mode is on.");
            debug = true;
        }
        else if args[i] == "-p" {
            if i+1 < args.len() {
                port = match args[i+1].parse::<usize>() {
                    Ok(_) => args[i+1].as_str(),
                    Err(_) => {
                        println!("Invalid port number, use 8000 as default port.");
                        "8000"
                    }
                };
            }
            else {
                println!("Port number can't be empty, use 8000 as default port.");
            }
        }
    }

    let addr = format!("0.0.0.0:{}", port);
    let listener = TcpListener::bind(addr).await;
    let listener = match listener {
        Ok(listener) => {
            println!("Established listener successfully on 0.0.0.0:{}.", port);
            listener},
        Err(_) => {
            println!("Unable to listen on 0.0.0.0:{}.", port);
            return;
        }
    };

    listener
        .incoming()
        .for_each_concurrent(/* limit */ None, |stream| async move {
            match stream {
                Ok(stream) => {
                    handle_tcp_stream(stream, debug).await;
                }
                Err(_) => {
                    if debug {
                        println!("Failed to get stream.");
                    }
                }
            }
        })
        .await;
}

async fn handle_tcp_stream(mut stream: TcpStream, debug: bool){
    let mut buffer = [0; 1024];
    stream.read(&mut buffer).await.unwrap();

    let http_request = String::from_utf8_lossy(&buffer[..]);
    let http_request = http_request.into_owned();
    let http_request_split = http_request.split("\r\n");
    let mut http_request:Vec<&str> = Vec::new();
    for line in http_request_split {
        if line.is_empty() {
            break;
        }
        http_request.push(line);
    }
    if debug {
        println!("Request: {:#?}", http_request);
    }

    let request_line:Option<&&str> = http_request.get(0);
    let request_line = match request_line {
        Some(line) => *line,
        None => {
            handle_500(stream, debug).await;
            return;
        },
    };
    // let request_line = request_line.split(" ");
    // let mut request_line_vec:Vec<&str> = Vec::new();
    // for line in request_line {
    //     if line.is_empty() {
    //         break;
    //     }
    //     let new_line = line.trim();
    //     request_line_vec.push(new_line);
    // }
    // if request_line_vec.len() != 3{
    //     handle_500(stream, &debug);
    //     return;
    // }
    let request_line = request_line.trim();
    let first_space_pos = request_line.find(' ');
    let last_space_pos = request_line.rfind(' ');
    let first_space_pos = match first_space_pos {
        Some(pos) => pos,
        None => request_line.len(),
    };
    let last_space_pos = match last_space_pos {
        Some(pos) => pos,
        None => request_line.len(),
    };

    if first_space_pos == request_line.len() || last_space_pos == request_line.len() {//incomplete request line
        handle_500(stream, debug).await;
        return;
    }
    
    let method = request_line[..first_space_pos].trim();

    if method != "GET" {
        handle_500(stream, debug).await;
        return;
    }

    let path = request_line[first_space_pos..last_space_pos].trim().to_string();
    let path = path.trim_matches('/').to_string();
    if path.len() == 0 {
        handle_500(stream, debug).await;
        return;
    }

    if debug { 
        println!("Path: {}", path); 
    }

    let last_dot_pos = path.rfind('.');
    let last_dot_pos = match last_dot_pos {
        Some(pos) => pos,
        None => path.len(),
    };

    let mut file_type = "";

    if path.len() > 0 && last_dot_pos < path.len()-1 {
        file_type = path[last_dot_pos+1..].trim();
    }

    if path.len() == 0 {
        handle_404(stream, debug).await;
    }
    else if last_dot_pos < path.len()-1 && file_type == "html" || file_type == "txt"
        || file_type == "htm" || file_type == "css" || file_type == "js" {
        let contents = fs::read_to_string(path);
        match contents {
            Ok(contents) => {
                handle_200_string(stream, contents, debug).await;
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream, debug).await;
                    return;
                }
                else {
                    handle_500(stream, debug).await;
                    return;
                }
            },
        };
    }
    else {
        let contents = fs::read(path);
        match contents {
            Ok(contents) => {
                handle_200_u8(stream, contents, debug).await;
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream, debug).await;
                    return;
                }
                else {
                    handle_500(stream, debug).await;
                    return;
                }
            },
        };
    }
}

async fn handle_404(mut stream: TcpStream, debug: bool) {
    let content = "404 NOT FOUND";
    let response = format!("HTTP/1.0 404 NOT FOUND \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    let result = stream.write_all(response.as_bytes()).await;
    match result {
        Ok(_) => {
            match stream.flush().await {
                Ok(_) => (),
                Err(_) => {
                    if debug { 
                        println!("Failed to flush stream.");
                    }
                }
            }
        }
        Err(_) => {
            if debug { 
                println!("Failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

async fn handle_500(mut stream: TcpStream, debug: bool) {
    let content = "500 Internal Server Error";
    let response = format!("HTTP/1.0 500 Internal Server Error \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    let result = stream.write_all(response.as_bytes()).await;
    match result {
        Ok(_) => {
            match stream.flush().await {
                Ok(_) => (),
                Err(_) => {
                    if debug { 
                        println!("Failed to flush stream.");
                    }
                }
            }
        }
        Err(_) => {
            if debug { 
                println!("Failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

async fn handle_200_u8(mut stream: TcpStream, contents: Vec<u8>, debug: bool) {
    let status_line = String::from("HTTP/1.0 200 OK");
    let length = contents.len();
    let response =
    format!("{status_line}\r\nContent-Length: {length}\r\n\r\n");
    let result = stream.write_all(response.as_bytes()).await;
    match result {
        Ok(_) => {
            match stream.flush().await {
                Ok(_) => (),
                Err(_) => {
                    if debug { 
                        println!("Failed to flush stream.");
                    }
                }
            }
        }
        Err(_) => {
            if debug { 
                println!("Failed to write to stream.");
            }
        }
    }
    let result = stream.write_all(&contents).await;
    match result {
        Ok(_) => {
            match stream.flush().await {
                Ok(_) => (),
                Err(_) => {
                    if debug { 
                        println!("Failed to flush stream.");
                    }
                }
            }
        }
        Err(_) => {
            if debug { 
                println!("Failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

async fn handle_200_string(mut stream: TcpStream, contents: String, debug: bool) {
    let status_line = String::from("HTTP/1.0 200 OK");
    let length = contents.len();
    let response =
        format!("{status_line}\r\nContent-Length: {length}\r\n\r\n{contents}");
        let result = stream.write_all(response.as_bytes()).await;
        match result {
            Ok(_) => {
                match stream.flush().await {
                    Ok(_) => (),
                    Err(_) => {
                        if debug { 
                            println!("Failed to flush stream.");
                        }
                    }
                }
            }
            Err(_) => {
                if debug { 
                    println!("Failed to write to stream.");
                }
            }
        }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}