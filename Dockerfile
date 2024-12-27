FROM archlinux
RUN pacman -Syu --noconfirm
RUN pacman -S base-devel --noconfirm
RUN pacman -S make --noconfirm
WORKDIR /app
COPY . .
EXPOSE 80
CMD ["./server", "80"]
