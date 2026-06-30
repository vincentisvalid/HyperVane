fn main() {
    prost_build::compile_protos(
        &["../../proto/hypervane.proto"],
        &["../../proto/"],
    )
    .expect("protobuf compilation failed");
}
