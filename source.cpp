#ifdef __APPLE__
#include <GLUT/glut.h> 
#else
#include <GL/glut.h> 
#endif

#include <stdlib.h>
#include <stddef.h>
#include <iostream>
#include <string.h>
#include <time.h>
#include <string>
#include <sstream>

#include <stdio.h>
#include <stdlib.h>

typedef uint8_t u8;						//8-bit ints used to represent colours

#define WIDTH 10
#define HEIGHT 22						//includes 2 top "hidden rows" from which we drop pieces
#define VISIBLE_HEIGHT 20

//patch for to_string on Linux

namespace patch
{
  template < typename T > std::string to_string (const T& n)
  {
    std::ostringstream stm;
    stm << n;
    return stm.str();
  }
}

/*---------VALUES--------*/

double speed;									//current speed
const double speedInitial = 500;				//initial speed at which the tetrino drops in milliseconds
const double speedFinal = 10;					//final speed at which the tetrino drops in milliseconds
const int speedTimer = 5000;					//speed increases every speedTimer milliseconds
int speedTick = 0;								//integer determining the next speed

const float ghostPieceAlpha = 0.3f;				//transparency of the ghostPiece

int score = 1;									//score. Increments per line cleared

/*-------FUNCTION DECLARATIONS--------*/

bool gameOver();								//true if the game is over. False otherwise
void reset();									//resets the game

/*-------GAME BOARD-------*/

u8 board[WIDTH * HEIGHT];				//8-bit board, storing different values for colour, empty space, and solid wall

//Note that row and col start at 0, and rows 0 and 1 are invisible. This means 0-21 rows, 0-9 columns
void setBoard(int row, int col, u8 value) {
	int index = row * WIDTH + col;
	board[index] = value;

}

u8 getBoard(int row, int col) {			//This is rows and cols down and to the right. (Note that row and col both start at 0.)
	if ((col < 0 || col >= WIDTH)) {	//Important for collision detection. Infinite walls surround the board left and right
		return 8;
	}
	if (row >= HEIGHT) {				//Infinite ground bottom of board
		return 8;
	}
	int index = row * WIDTH + col;
	return board[index];
}

/*--------TETRINO-------*/

struct Tetrino {
	const u8 *data;						//Data telling us what the tetrino shape looks like and its colour
	int side;							//Number telling us the dimensions of the data (size of square matrix)
};

//Tetrino constructor. Takes a square matrix and the size of the matrix holding the tetrino
inline Tetrino tetrino(const u8 *data, int side){
	return {data, side};
}

//Getter for the index of the matrix representing the tetrino. Rotations are handled by changing indexing, as you can see below
inline u8 tetrino_get(Tetrino &tetrino, int row, int col, int rotation) {
	int side = tetrino.side;
	switch (rotation) {
	case 0: //No rotation
		return tetrino.data[row * side + col]; break;
	case 1: //90 degree clockwise
		return tetrino.data[(side - col - 1) * side + row]; break;
	case 2: //180 degrees clockwise
		return tetrino.data[(side - row - 1) * side + (side - col - 1)]; break;
	case 3: //270 degrees clockwise
		return tetrino.data[col * side + (side - row - 1)]; break;
	} return 0; //For good measure
}

/*-------BITMAP TEXT-------*/
unsigned int g_bitmap_text_handle = 0;

unsigned int make_bitmap_text()

{
	unsigned int handle_base = glGenLists(256);

	for (size_t i = 0; i < 256; i++)
	{
		// a new list for each character
		glNewList(handle_base + i, GL_COMPILE);
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, i);
		glEndList();
	}
	return handle_base;
}



void draw_text(const char* text)
{
	glListBase(g_bitmap_text_handle);
	glCallLists(int(strlen(text)), GL_UNSIGNED_BYTE, text);
}

/*-------PIECE DATA-------*/
const u8 TETRINO_1[]{
	0,0,0,0,
	1,1,1,1,
	0,0,0,0,
	0,0,0,0
};

const u8 TETRINO_2[]{
	2,2,
	2,2
};

const u8 TETRINO_3[]{
	0,0,0,
	3,3,3,
	0,3,0
};

const u8 TETRINO_4[]{
	4,0,0,
	4,4,0,
	0,4,0
};

const u8 TETRINO_5[]{
	0,5,0,
	0,5,0,
	0,5,5
};

const u8 TETRINO_6[]{
	0,6,0,
	0,6,0,
	6,6,0
};

const u8 TETRINO_7[]{
	0,0,7,
	0,7,7,
	0,7,0
};

const Tetrino TETRINOS[] = {
	tetrino(TETRINO_1, 4),
	tetrino(TETRINO_2, 2),
	tetrino(TETRINO_3, 3),
	tetrino(TETRINO_4, 3),
	tetrino(TETRINO_5, 3),
	tetrino(TETRINO_6, 3),
	tetrino(TETRINO_7, 3)
};

struct pieceState {						//Defines the type, position and rotation of the current piece
	u8 TetrinoIndex = 0;
	int offset_row = 2;
	int offset_col = 2;
	int rotation = 0;
} state, nextState, ghostState;			//Defines objects for the state of the current piece, next piece and the ghost piece.

inline u8 randomNumber() {				//this function is called in the creation of a new tetrino. Picks a random one
	return rand() % 7;					//return number between 0 and 6
}

//Initiate tetrinos
Tetrino currentTetrino = TETRINOS[randomNumber()];									//Tetrino that's dropping
Tetrino nextTetrino = TETRINOS[randomNumber()];										//The tetrino that's next in line
Tetrino ghostTetrino = currentTetrino;												//The ghost tetrino telling you where the current tetrino will land

/*-------DRAWING METHODS FOR BOARD AND BLOCKS-------*/

//Draw game board background (white)
void draw_background() {
	static float vertex[4][2] =
	{
		{0.0f, 0.0f},
		{10.0f, 0.0f},
		{10.0f, 20.0f},
		{0.0f, 20.0f}
	};

	glBegin(GL_POLYGON);
	for (size_t i = 0; i < 4; i++)
		glVertex2fv(vertex[i]);
	glEnd();
}

//Draw_square function
void draw_square()
{
	// in model coordinates centred at (0,0)
	static float vertex[4][2] =
	{
		{-1.0f, -1.0f},
		{1.0f, -1.0f},
		{1.0f, 1.0f},
		{-1.0f, 1.0f}
	};

	glBegin(GL_POLYGON);
	for (size_t i = 0; i < 4; i++)
		glVertex2fv(vertex[i]);
	glEnd();
}

/*-------GAME LOGIC--------*/

bool collision(pieceState foo) {						//general collision tester
	for (int i = foo.offset_col; i < foo.offset_col + currentTetrino.side; i++) {
		for (int j = foo.offset_row; j < foo.offset_row + currentTetrino.side; j++) {
			int value1 = tetrino_get(currentTetrino, j - foo.offset_row, i - foo.offset_col, foo.rotation);
			int value2 = getBoard(j, i);
			if ((value1 != 0) && (value2 != 0)) {		//collision of tetrino with the board or the walls
				return true;
			}
		}
	}
	return false;										//no collisions found
}

bool collisionBottom(pieceState foo, Tetrino tetrino) {					//specialized collision tester to see if the tetrino is resting on something
	for (int i = foo.offset_col; i < foo.offset_col + tetrino.side; i++) {
		for (int j = foo.offset_row; j < foo.offset_row + tetrino.side; j++) {
			int value1 = tetrino_get(tetrino, j - foo.offset_row, i - foo.offset_col, foo.rotation);
			int value2 = getBoard(j + 1, i);							//We compare with the block right below the tetrino
			if ((value1 != 0) && (value2 != 0)) {
				return true;
			}
		}
	}
	return false;														//no collision with bottom
}

//wallBounce is a function that allows rotations of the tetrino near a boundary, by pushing the piece left or right if necessary (and allowed)
void wallBounce(pieceState foo) {
	foo.offset_col = state.offset_col - 1;
	if (collision(foo) == false) {
		state = foo; return;
	}

	foo.offset_col = state.offset_col + 1;
	if (collision(foo) == false) {
		state = foo; return;
	}

	foo.offset_col = state.offset_col - 2;
	if (collision(foo) == false) {
		state = foo; return;
	}

	foo.offset_col = state.offset_col + 2;
	if (collision(foo) == false) {
		state = foo; return;
	}
}

void updateBoard(int) {													//updates the board and tetrino when the tetrino hits the bottom for a certain amount of time
	if (gameOver() == false) {
		if (collisionBottom(state, currentTetrino) == true) {
			/*UPDATE BOARD (This loop saves the tetrino into the board array)*/
			for (int i = state.offset_col; i < state.offset_col + currentTetrino.side; i++) {
				for (int j = state.offset_row; j < state.offset_row + currentTetrino.side; j++) {
					int colour = tetrino_get(currentTetrino, j - state.offset_row, i - state.offset_col, state.rotation);
					if (colour != 0) {
						setBoard(j, i, colour);
					}
				}
			}
		//UPDATE PIECE//
		u8 nextPieceIndex = randomNumber();
		state.offset_col = 4;
		state.offset_row = -1;

		//Updates current piece and piece look-ahead
		currentTetrino.data = nextTetrino.data;
		currentTetrino.side = nextTetrino.side;
		nextTetrino.data = TETRINOS[nextPieceIndex].data;
		nextTetrino.side = TETRINOS[nextPieceIndex].side;
		}
	}
}

void dropPiece(pieceState &foo, Tetrino tetrino) {							//drops the tetrino when pressing the spacebar
	while (collisionBottom(foo, tetrino) == false) {
		foo.offset_row += 1;
	}
}

void fallPiece(int) {														//moves the tetrino downward when "down key" is pressed
	if (collisionBottom(state, currentTetrino) == false) {
		state.offset_row += 1;
	}
	glutPostRedisplay();
	glutTimerFunc(speed, fallPiece, 0);
}

//Changes the difficulty over time (i.e. the speed)
void difficulty(int i) {
	speed = (speedInitial-speedFinal)/((double)speedTick*speedTick/200+1)+speedFinal;	//Speed will tend to speedFinal milliseconds per drop, and starts at speedInitial milliseconds per drop
	speedTick++;
	glutTimerFunc(speedTimer, difficulty, 0);											//speed increases every speedTimer seconds
}

void clearRow() {
	for (int i = HEIGHT-VISIBLE_HEIGHT; i < HEIGHT; i++) {
		bool fullRow = true;							//is row i full?
		for (int j = 0; j < WIDTH; j++) {
			if (getBoard(i, j) == 0) {
				fullRow = false;
			}
		}
		if (fullRow == true) {							//if the row is full, shift everything down above the row
			score += 1;									//add one to the score.
			for (int j = i-1; j >= 0; j--) {			//rows above row i
				for (int k = 0; k < WIDTH; k++) {
					setBoard(j+1, k, getBoard(j, k));
				}
			}
		}
	}
}

bool gameOver() {											//simple check that tests whether there are blocks placed in the hidden rows above the view space
	for (int j = 0; j < WIDTH; j++) {
		if (getBoard(1,j) != 0) {
			return true;
		}
	}
	return false;
}

void updateGhostTetrino() {									//has to be continuously updated to make sure the ghostTetrino aligns with the currentTetrino
	ghostTetrino = currentTetrino;
	ghostState = state;
	dropPiece(ghostState, ghostTetrino);
}

void gameLogic(int) {
		updateGhostTetrino();
		if (collisionBottom(state, currentTetrino) == true) {
			glutTimerFunc(750, updateBoard, 0);					//places the current piece on the board if it touches zero
		}
		clearRow();												//clears rows when it detects a full row
		glutPostRedisplay();									//force redisplay
		glutTimerFunc(50, gameLogic, 0);						//call gameLogic every 25 milliseconds
}

void reset()													//resets or restarts the game

{
	for (int i = 0; i < WIDTH; i++) {
		for (int j = 0; j < HEIGHT; j++) {
			setBoard(j, i, 0);
		}
	}
	speed = speedInitial;
	score = 1;
	Tetrino currentTetrino = TETRINOS[randomNumber()];									//Tetrino that's dropping
	Tetrino nextTetrino = TETRINOS[randomNumber()];										//The tetrino that's waiting
	Tetrino ghostTetrino = currentTetrino;												//The ghost tetrino telling you where it'll drop
	speedTick = 0;																		//reset speed
	glutPostRedisplay();
}

/*--------DISPLAY--------*/

void display()

{
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_MODELVIEW);

	//draw a white rectangle for the game grid
	glColor3f(1.0f, 1.0f, 1.0f);
	glPushMatrix();
	glScalef(1.0f, 1.0f, 1.0f);
	draw_background();
	glPopMatrix();

	//draw next tetrino
	for (int i = 0; i < nextTetrino.side; i++) {
		for (int j = 0; j < nextTetrino.side; j++) {
			int value = tetrino_get(nextTetrino, j, i, nextState.rotation);
			if (value != 0) {
				switch (value) {
				case 1: glColor3f(0.531f, 0.086f, 0.000f); break;
				case 2: glColor3f(0.305f, 0.000f, 0.063f); break;
				case 3: glColor3f(0.754f, 0.038f, 0.000f); break;
				case 4: glColor3f(0.887f, 0.887f, 0.414f); break;
				case 5: glColor3f(1.000f, 0.957f, 0.547f); break;
				case 6: glColor3f(0.883f, 0.648f, 0.414f); break;
				case 7: glColor3f(1.000f, 0.668f, 0.148f); break;
				default: break;
				}

				glPushMatrix();
				// place block on the grid
				glTranslatef(1.0*i + 15.5f - 0.5*nextTetrino.side, HEIGHT - 1.0*j - 5.0f, 0.0f);
				// scale the block appropriately
				glScalef(0.5f, 0.5f, 1.0f);

				draw_square();

				glPopMatrix();
			}
		}
	}

	//draw current tetrino. We do not actually add it to the board itself, so this is done separately
	for (int i = 0; i < currentTetrino.side; i++) {
		for (int j = 0; j < currentTetrino.side; j++) {
			int value = tetrino_get(currentTetrino, j, i, state.rotation);
			if (value != 0) {
				switch (value) {
				case 1: glColor3f(0.531f, 0.086f, 0.000f); break;
				case 2: glColor3f(0.305f, 0.000f, 0.063f); break;
				case 3: glColor3f(0.754f, 0.038f, 0.000f); break;
				case 4: glColor3f(0.887f, 0.887f, 0.414f); break;
				case 5: glColor3f(1.000f, 0.957f, 0.547f); break;
				case 6: glColor3f(0.883f, 0.648f, 0.414f); break;
				case 7: glColor3f(1.000f, 0.668f, 0.148f); break;
				default: break;
				}

				glPushMatrix();
				// place block on the grid
				glTranslatef(0.5f + i + state.offset_col, HEIGHT - 0.5f - j - state.offset_row, 0.0f);
				// scale the block appropriately
				glScalef(0.5f, 0.5f, 1.0f);

				draw_square();

				glPopMatrix();
			}
		}
	}

	//draw ghost tetrino
	for (int i = 0; i < ghostTetrino.side; i++) {
		for (int j = 0; j < ghostTetrino.side; j++) {
			int value = tetrino_get(ghostTetrino, j, i, ghostState.rotation);
			if (value != 0) {
				switch (value) {
				case 1: glColor4f(0.531f, 0.086f, 0.000f, ghostPieceAlpha); break;
				case 2: glColor4f(0.305f, 0.000f, 0.063f, ghostPieceAlpha); break;
				case 3: glColor4f(0.754f, 0.038f, 0.000f, ghostPieceAlpha); break;
				case 4: glColor4f(0.887f, 0.887f, 0.414f, ghostPieceAlpha); break;
				case 5: glColor4f(1.000f, 0.957f, 0.547f, ghostPieceAlpha); break;
				case 6: glColor4f(0.883f, 0.648f, 0.414f, ghostPieceAlpha); break;
				case 7: glColor4f(1.000f, 0.668f, 0.148f, ghostPieceAlpha); break;
				default: break;
				}

				glPushMatrix();
				// place block on the grid
				glTranslatef(0.5f + i + ghostState.offset_col, HEIGHT - 0.5f - j - ghostState.offset_row, 0.0f);
				// scale the block appropriately
				glScalef(0.5f, 0.5f, 1.0f);

				draw_square();

				glPopMatrix();
			}
		}
	}

	//draw the board. This draws directly from the board array
	for (int i = 0; i < WIDTH; i++) {
		for (int j = 0; j < HEIGHT; j++) {
			if (getBoard(j, i) != 0) {
				switch (getBoard(j, i)) {
				case 1: glColor3f(0.531f, 0.086f, 0.000f); break;
				case 2: glColor3f(0.305f, 0.000f, 0.063f); break;
				case 3: glColor3f(0.754f, 0.038f, 0.000f); break;
				case 4: glColor3f(0.887f, 0.887f, 0.414f); break;
				case 5: glColor3f(1.000f, 0.957f, 0.547f); break;
				case 6: glColor3f(0.883f, 0.648f, 0.414f); break;
				case 7: glColor3f(1.000f, 0.668f, 0.148f); break;
				default: break;
				}
				glPushMatrix();
				// translate block on the grid
				glTranslatef(0.5f + i, HEIGHT - 0.5f - j, 0.0f);
				// scale the block appropriately
				glScalef(0.5f, 0.5f, 1.0f);
				// render the block
				draw_square();
				glPopMatrix();
			}
		}
	}

	//Draw Text
	glColor3f(1.0f, 1.0f, 1.0f);
	glPushMatrix();
	glRasterPos2i(12.0f, 18.8f);
	draw_text("NEXT PIECE:");
	glRasterPos2i(12.0f, 14.0f);
	draw_text("SCORE: ");
	std::string scoreString = patch::to_string(score);
	char const* pchar = scoreString.c_str();

	draw_text(pchar);

	if (gameOver() == true) {
		glRasterPos2i(12.0f, 9.2f);
		draw_text("GAME OVER");
		glRasterPos2i(12.0f, 8.0f);
		draw_text("Press 'x' to restart");
		glRasterPos2i(12.0f, 7.4f);
		draw_text("Press 'q' to quit");
		glPopMatrix();
	}

	glutSwapBuffers();
}

/*---------KEYBOARD FUNCTIONS--------*/
void keyboard(unsigned char key, int, int) {
	if (gameOver() == false) {
		pieceState tempState = state;						//temporary pieceState, on which we do collision detection before we move
		switch (key) {
		case 'r': {
			tempState.rotation += 1;						//rotate right
			if (tempState.rotation > 3) {
				tempState.rotation = 0;
			}

			if (collision(tempState) == false) {
				state = tempState; break;
			}
			else {											//try to bounce the piece off the walls if it's possible
				wallBounce(tempState); break;
			}
		}
		case 'e': {
			tempState.rotation -= 1;						//rotate left
			if (tempState.rotation < 0) {
				tempState.rotation = 3;
			}

			if (collision(tempState) == false) {
				state = tempState; break;
			}
			else {
				wallBounce(tempState); break;				//try to bounce the piece off the walls if it's possible
			}
		}

		case 32: //spacebar
			dropPiece(tempState, currentTetrino);			//drop the piece
			state = tempState;
			updateBoard(0);
			break;
		}
	}
	if (gameOver() == true) {
		switch (key) {
		case 'q': exit(1); break;							//exit the program
		case 'x': reset(); break;							//reset the game (restart)
		}
	}

	updateGhostTetrino();

	glutPostRedisplay(); //force a redraw
}

void special(int key, int, int) {
	if (gameOver() == false) {
		pieceState tempState = state;						//temporary pieceState, on which we do collision detection before we move
		switch (key) {
		case GLUT_KEY_LEFT: tempState.offset_col -= 1; break;
		case GLUT_KEY_RIGHT: tempState.offset_col += 1; break;
		case GLUT_KEY_DOWN: tempState.offset_row += 1; break;
		}

		if (collision(tempState) == false) {
			state = tempState;
		}
		updateGhostTetrino();
		glutPostRedisplay(); //force a redraw
	}
}

/*------GLUT INCANTATIONS------*/

void init()
{
	srand(time(NULL));

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, 20, 0, 20);						//Interpret as: dimensions of the viewport are 20x20 "blocks". The board itself is 20x10 blocks
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	g_bitmap_text_handle = make_bitmap_text();
}

int main(int argc, char* argv[])
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(500, 500);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("Tetris");
	glutDisplayFunc(display);
	if (gameOver() == false) {
		gameLogic(0);
		fallPiece(0);
		difficulty(0);
	}

	//Handlers for character and arrow keys
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(special);

	init();
	glutMainLoop();

	return 0;

}

