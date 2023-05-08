use std::{
    io, 
    io::{prelude::*, BufReader},
    fs,
    net::{TcpListener, TcpStream},
};

fn main() {
    let listener = loop{
        let listener = TcpListener::bind("0.0.0.0:8000");
        match listener {
            Ok(listener) => {
                println!("Established listener successfully!");
                break listener},
            Err(_) => {
                println!("Unable to listen on 0.0.0.0:8000.");
                println!("Enter Y/y to try again, or enter any other keys to exit.");
                let mut choice = String::new();
                io::stdin()
                    .read_line(&mut choice)
                    .expect("Failed to read line.");
                let choice_str = choice.trim();
                if choice_str == "Y" || choice_str == "y"{
                    continue;
                }
                else {
                    return;
                }
            }
        }
    };

    for stream in listener.incoming() {
        let stream = stream.unwrap();
        handle_tcp_stream(stream);
    }
}

fn handle_tcp_stream(mut stream: TcpStream){
    let buf_reader = BufReader::new(& mut stream);
    let mut http_requests:Vec<String> = Vec::new();
    let buf_reader_lines = buf_reader.lines();
    for line in buf_reader_lines {
        let new_line = match line {
            Ok(line) => line,
            Err(_) => {
                handle_500(stream);
                return;
            },
        };
        if new_line.is_empty() {
            break;
        }
        http_requests.push(new_line);
    }
    println!("Request: {:#?}", http_requests);

    let request_line:Option<&String> = http_requests.get(0);
    let request_line = match request_line {
        Some(line) => line,
        None => {
            handle_500(stream);
            return;
        },
    };
    let request_line = request_line.split(" ");
    let mut request_line_vec:Vec<&str> = Vec::new();
    for line in request_line {
        if line.is_empty() {
            break;
        }
        let new_line = line.trim();
        request_line_vec.push(new_line);
    }
    if request_line_vec.len() != 3{
        handle_500(stream);
        return;
    }
    
    let method = request_line_vec.get(0);
    let method = match method {
        Some(method) => *method,
        None => {
            handle_500(stream);
            return;
        },
    };

    if method != "GET" {
        handle_500(stream);
        return;
    }

    let path = request_line_vec.get(1);
    let mut path = match path {
        Some(path) => (*path).to_string(),
        None => {
            handle_500(stream);
            return;
        },
    };
    path.remove(0);

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
        handle_404(stream);
    }
    else if last_dot_pos < path.len()-1 && file_type == "html" || file_type == "txt"
        || file_type == "htm" || file_type == "css" || file_type == "js" {
        let contents = fs::read_to_string(path);
        match contents {
            Ok(contents) => {
                let status_line = String::from("HTTP/1.0 200 OK");
                let length = contents.len();
                let response =
                    format!("{status_line}\r\nContent-Length: {length}\r\n\r\n{contents}");
                stream.write_all(response.as_bytes()).unwrap();
                println!("Respond: [\r\n{response}\r\n]");
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream);
                    return;
                }
                else {
                    handle_500(stream);
                    return;
                }
            },
        };
    }
    else {
        let contents = fs::read(path);
        match contents {
            Ok(contents) => {
                let status_line = String::from("HTTP/1.0 200 OK");
                let length = contents.len();
                let response =
                    format!("{status_line}\r\nContent-Length: {length}\r\n\r\n");
                stream.write_all(response.as_bytes()).unwrap();
                stream.write_all(&contents).unwrap();
                println!("Respond: [\r\n{response}\r\n]");
            },
            Err(error) => {//file not found
                if error.kind() == std::io::ErrorKind::NotFound {
                    handle_404(stream);
                    return;
                }
                else {
                    handle_500(stream);
                    return;
                }
            },
        };
    }
}

fn handle_404(mut stream: TcpStream) {
    let content = "404 NOT FOUND";
    let response = format!("HTTP/1.0 404 NOT FOUND \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    stream.write_all(response.as_bytes()).unwrap();
    println!("Respond: [\r\n{response}\r\n]");
}

fn handle_500(mut stream: TcpStream) {
    let content = "500 Internal Server Error";
    let response = format!("HTTP/1.0 500 Internal Server Error \r\nContent-Length: {}\r\n\r\n{}",
        content.len(), content);
    stream.write_all(response.as_bytes()).unwrap();
    println!("Respond: [\r\n{response}\r\n]");
}