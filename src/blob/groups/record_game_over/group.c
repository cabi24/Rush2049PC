/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
extern int gRace;
extern void RecordRace(void);
extern void RecordPractice(void);
extern void RecordGhost(void);
extern void RecordStunt(void);
extern void RecordObstacle(void);
extern void RecordBattle(void);
void RecordGameOver(void)
{
    switch(gRace) {
    case 0:case 3:RecordRace();break;
    case 1:RecordPractice();break;
    case 2:RecordGhost();break;
    case 4:RecordStunt();break;
    case 5:RecordObstacle();break;
    case 6:RecordBattle();break;
    }
}
