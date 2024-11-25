FROM archlinux
RUN pacman -Syu --noconfirm
COPY . .
EXPOSE 80
CMD ["./server", "80"]
