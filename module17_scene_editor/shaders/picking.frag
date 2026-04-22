#version 460 core
uniform uint uEntityId;
out uint FragColor;
void main() {
    FragColor = uEntityId;
}
