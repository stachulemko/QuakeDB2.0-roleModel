#include <iostream>
#include <thread>  // Dodaj ten include
#include <chrono>
#include "roleThreadManager.h"
#include "../../bufforing-stm/src/buserCache.h"
#include "../../bufforing-stm/src/buserProcess.h"

int main(){
    // Example usage
    std::cout<<"t123est"<<std::endl;
    startSession("adminQkDB", "Quake17",3600);
    addBuser("newUser", "newPassword","stachulemko@o2.pl",false,"adminQkDB","Quake17");
    waitForAllProcessesToFinish();
    // Teraz addBuser czeka na zakończenie zadania
    showUserCache();  // Wyświetl cache po przetworzeniu
    showProcessBuffer();
}