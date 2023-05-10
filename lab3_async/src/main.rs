use std::{
    io::{prelude::*, BufReader},
    fs,
    net::{TcpListener, TcpStream},
    env,//fmt::{format, Debug},
};

fn main() {
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
    let listener = TcpListener::bind(addr);
    let listener = match listener {
        Ok(listener) => {
            println!("Established listener successfully on 0.0.0.0:{}.", port);
            listener},
        Err(_) => {
            println!("Unable to listen on 0.0.0.0:{}.", port);
            return;
        }
    };

    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                handle_tcp_stream(stream, debug);
            }
            Err(_) => {
                if debug {
                    println!("Failed to get stream.");
                }
            }
        }
        
    }
}

fn handle_tcp_stream(mut stream: TcpStream, debug: bool){
    let buf_reader = BufReader::new(& mut stream);
    let mut http_request:Vec<String> = Vec::new();
    let buf_reader_lines = buf_reader.lines();//read request
    for line in buf_reader_lines {
        let new_line = match line {
            Ok(line) => line,
            Err(_) => {
                handle_500(stream, debug);
                return;
            },
        };
        if new_line.is_empty() {
            break;
        }
        http_request.push(new_line);//save request in vector
    }
    if debug {
        println!("Request: {:#?}", http_request);
    }

    let request_line:Option<&String> = http_request.get(0);
    let request_line = match request_line {
        Some(line) => line,
        None => {
            handle_500(stream, debug);
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
        handle_500(stream, debug);
        return;
    }
    
    let method = request_line[..first_space_pos].trim();

    if method != "GET" {
        handle_500(stream, debug);
        return;
    }

    let path = request_line[first_space_pos..last_space_pos].trim();
    if path.len() == 0 {
        handle_500(stream, debug);
        return;
    }

    let mut path = path.to_string();

    if path[0..1].to_string() == "/" {
        path.remove(0);
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
        handle_404(stream, debug);
    }
    else if last_dot_pos < path.len()-1 && file_type == "html" || file_type == "txt"
        || file_type == "htm" || file_type == "css" || file_type == "js" {
        let contents = fs::read_to_string(path);
        match contents {
            Ok(contents) => {
                handle_200_string(stream, contents, debug);
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream, debug);
                    return;
                }
                else {
                    handle_500(stream, debug);
                    return;
                }
            },
        };
    }
    else {
        let contents = fs::read(path);
        match contents {
            Ok(contents) => {
                handle_200_u8(stream, contents, debug);
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream, debug);
                    return;
                }
                else {
                    handle_500(stream, debug);
                    return;
                }
            },
        };
    }
}

fn handle_404(mut stream: TcpStream, debug: bool) {
    let content = "404 NOT FOUND";
    let response = format!("HTTP/1.0 404 NOT FOUND \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

fn handle_500(mut stream: TcpStream, debug: bool) {
    let content = "500 Internal Server Error";
    let response = format!("HTTP/1.0 500 Internal Server Error \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

fn handle_200_u8(mut stream: TcpStream, contents: Vec<u8>, debug: bool) {
    let status_line = String::from("HTTP/1.0 200 OK");
    let length = contents.len();
    let response =
    format!("{status_line}\r\nContent-Length: {length}\r\n\r\n");
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    let result = stream.write_all(&contents);
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}

fn handle_200_string(mut stream: TcpStream, contents: String, debug: bool) {
    let status_line = String::from("HTTP/1.0 200 OK");
    let length = contents.len();
    let response =
        format!("{status_line}\r\nContent-Length: {length}\r\n\r\n{contents}");
    let result = stream.write_all(response.as_bytes());
    match result {
        Ok(_) => (),
        Err(_) => {
            if debug { 
                println!("failed to write to stream.");
            }
        }
    }
    if debug {
        println!("Respond: [\r\n{response}\r\n]");
    }
}