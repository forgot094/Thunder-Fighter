#include "raylib.h"
#include <stdbool.h>

#define MAX_BULLETS 256
#define MAX_ENEMIES 64
#define MAX_STARS 150
#define MAX_PARTICLES 300

typedef struct { Vector2 pos;Vector2 vel; bool active; } Bullet;
typedef struct { Vector2 pos; bool active; float speed; } Enemy;
// （1=回血，2=火力升级）
typedef struct { Vector2 pos; bool active; int type; float speed; } Item;
#define MAX_ITEMS 10 // 屏幕上最多同时存在 10 个道具
//Boss 的结构体
typedef struct {
	Vector2 pos;
	bool active;
	int hp;
	int maxHp;
	float shootTimer;
	float moveDir; // 1 向右，-1 向左
} Boss;

#define MAX_BOSS_BULLETS 100

struct Particle {
	Vector2 pos;      // 位置
	Vector2 vel;      // 速度
	float life;       // 剩余生命值
	float maxLife;    // 初始生命值
	float size;       // 大小
	Color color;      // 颜色
};

int main(void) {
	InitWindow(480, 800, "Thunder Fighter");
	SetTargetFPS(60);

	// 图片加载
	Texture2D playerTex = LoadTexture("airplane.png");
	Texture2D enemyTex = LoadTexture("enemy.png");
	Texture2D bulletTex = LoadTexture("bullet.png");
	Texture2D bossTex = LoadTexture("boss.png");

	// 声音加载
	InitAudioDevice();
	Sound shootSound = LoadSound("shoot.wav");
	Sound explosionSound = LoadSound("explosion.wav");
	SetSoundVolume(shootSound, 0.2f);
	SetSoundVolume(explosionSound, 0.1f);

	// 变量初始化
	Vector2 player = { 240, 700 };
	int playerHP = 100;
	bool gameover = false;
	float invincibleTimer = 0.0f;
	Bullet bullets[MAX_BULLETS] = { 0 };
	Enemy enemies[MAX_ENEMIES] = { 0 };
	Item items[MAX_ITEMS] = { 0 };
	float powerUpTimer = 0.0f; // 火力升级剩余时间
	int score = 0;
	int difficultyScore = 0; // 👈 专门用来计算难度的分数
	float spawnTimer = 0;

	Vector2 stars[MAX_STARS];
	float starSpeeds[MAX_STARS];
	float starSizes[MAX_STARS];
	Particle particles[MAX_PARTICLES] = { 0 };
	// Boss 相关的变量
	Boss myBoss = { 0 };
	Bullet bossBullets[MAX_BOSS_BULLETS] = { 0 };
	bool isBossStage = false; // 是否进入 Boss 战
	int nextBossScore = 500;//第一个boss在500分时出现


	// 初始化星星
	for (int i = 0; i < MAX_STARS; i++) 
	{
		stars[i].x = (float)GetRandomValue(0, 480);
		stars[i].y = (float)GetRandomValue(0, 800);
		starSpeeds[i] = (float)GetRandomValue(50, 200);
		starSizes[i] = (float)GetRandomValue(1, 3);
	}

	while (!WindowShouldClose())
	{
		float dt = GetFrameTime();

		// ==================== 逻辑更新区 ====================
		if (!gameover)
		{
			// 1. 更新星空位置
			for (int i = 0; i < MAX_STARS; i++)
			{
				stars[i].y += starSpeeds[i] * dt;
				if (stars[i].y > 800.0f)
				{
					stars[i].y = -5.0f;
					stars[i].x = (float)GetRandomValue(0, 480);
				}
			}

			// 2. 更新无敌时间
			if (invincibleTimer > 0.0f)
			{
				invincibleTimer -= dt;
			}

			// 3. 玩家移动
			if (IsKeyDown(KEY_LEFT))  player.x -= 300 * dt;
			if (IsKeyDown(KEY_RIGHT)) player.x += 300 * dt;
			if (IsKeyDown(KEY_UP))    player.y -= 300 * dt;
			if (IsKeyDown(KEY_DOWN))  player.y += 300 * dt;
			if (player.x < 20) player.x = 20;
			if (player.x > 460) player.x = 460;
			if (player.y < 20) player.y = 20;
			if (player.y > 780) player.y = 780;

			// 4. 射击
			if (IsKeyPressed(KEY_SPACE))
			{
				PlaySound(shootSound);

				if (powerUpTimer > 0.0f) 
				{
					// 🔥 火力升级状态：发射双排子弹！
					int bulletsFired = 0;
					for (int i = 0; i < MAX_BULLETS; i++) 
					{
						if (!bullets[i].active) 
						{
							bullets[i].active = true;
							// 注意这里的 x 是 player.x - 15.0f（左边那颗）
							bullets[i].pos = Vector2{ player.x - 15.0f, player.y - 20 };
							bulletsFired++;
							if (bulletsFired == 1) 
							{
								// 稍微修改一下逻辑，利用 i+1 直接生成右边那颗
								for (int j = i + 1; j < MAX_BULLETS; j++) 
								{
									if (!bullets[j].active) 
									{
										bullets[j].active = true;
										bullets[j].pos = Vector2{ player.x + 15.0f, player.y - 20 };
										break;
									}
								}
								break;
							}
						}
					}
				}
				else 
				{
					// 🟢 普通状态：单发子弹
					for (int i = 0; i < MAX_BULLETS; i++) 
					{
						if (!bullets[i].active) 
						{
							bullets[i].active = true;
							bullets[i].pos = Vector2{ player.x, player.y - 20 };
							break;
						}
					}
				}
			}

			// 5. 更新子弹
			for (int i = 0; i < MAX_BULLETS; i++) 
			{
				if (bullets[i].active) 
				{
					bullets[i].pos.y -= 500 * dt;
					if (bullets[i].pos.y < -10) bullets[i].active = false;
				}
			}

			// 6. 生成敌机（动态难度）
			spawnTimer += dt;

			// 触发 Boss 战 👇👇👇
			// 如果分数达到 500 且 Boss 还没出现，召唤 Boss！
			if (score >= nextBossScore && !myBoss.active && !isBossStage)
			{
				isBossStage = true;
				myBoss.active = true;
				myBoss.pos = Vector2{ 240, -100 }; // 从屏幕正上方掉下来
				myBoss.hp = 1000;
				myBoss.maxHp = 1000;
				myBoss.moveDir = 1.0f;
				myBoss.shootTimer = 0;
			}

			if (!isBossStage) 
			{
				// 核心公式：基础0.8秒，每得100分缩短0.1秒，最快不能低于0.3秒
				float currentSpawnInterval = 0.8f - (difficultyScore / 100.0f) * 0.05f;
				if (currentSpawnInterval < 0.3f)
				{
					currentSpawnInterval = 0.3f;
				}

				if (spawnTimer > currentSpawnInterval)
				{
					spawnTimer = 0;
					for (int i = 0; i < MAX_ENEMIES; i++)
					{
						if (!enemies[i].active)
						{
							enemies[i].active = true;
							enemies[i].pos = Vector2{ (float)GetRandomValue(40, 440), -20.0f };
							// 随着分数增加，速度加快
							float speedBonus = (difficultyScore / 100.0f) * 20.0f;
							enemies[i].speed = (float)GetRandomValue(100, 200) + speedBonus;
							break;
						}
					}
				}
			}

			// 7. 更新敌机
			for (int i = 0; i < MAX_ENEMIES; i++)
			{
				if (enemies[i].active)
				{
					enemies[i].pos.y += enemies[i].speed * dt;
					if (enemies[i].pos.y > 820) enemies[i].active = false;
				}
			}
		 // 更新火力升级时间
			if (powerUpTimer > 0.0f) powerUpTimer -= dt;

			for (int i = 0; i < MAX_ITEMS; i++) 
			{
				if (items[i].active) 
				{
					items[i].pos.y += items[i].speed * dt; // 往下掉

					// 飞出屏幕消失
					if (items[i].pos.y > 800) items[i].active = false;

					// 检测玩家是否吃到（半径 20 的圆判定）
					if (CheckCollisionCircles(player, 20.0f, items[i].pos, 15.0f)) 
					{
						items[i].active = false; // 道具消失
						if (items[i].type == 1) 
						{
							playerHP += 20; // 回血
							if (playerHP > 100) playerHP = 100;
						}
						else if (items[i].type == 2) 
						{
							powerUpTimer = 8.0f; // 火力升级持续 8 秒
						}
					}
				}
			}
			// 👇 更新 Boss 👇
			if (myBoss.active) {
				// 1. 掉落到指定高度 (Y=100)
				if (myBoss.pos.y < 100.0f) 
				{
					myBoss.pos.y += 50 * dt;
				}
				else 
				{
					// 2. 左右移动
					myBoss.pos.x += myBoss.moveDir * 80 * dt;
					if (myBoss.pos.x > 400) myBoss.moveDir = -1;
					if (myBoss.pos.x < 80) myBoss.moveDir = 1;

					// 3. 发射扇形弹幕
					myBoss.shootTimer += dt;
					if (myBoss.shootTimer > 1.2f) 
					{
						myBoss.shootTimer = 0;
						// 一次发射 5 颗子弹，呈扇形散开
						for (int b = 0; b < 5; b++) 
						{
							for (int k = 0; k < MAX_BOSS_BULLETS; k++) 
							{
								if (!bossBullets[k].active) 
								{
									bossBullets[k].active = true;
									bossBullets[k].pos = myBoss.pos;
									// 计算扇形速度
									bossBullets[k].vel.x = (b - 2) * 80.0f; // -160, -80, 0, 80, 160
									bossBullets[k].vel.y = 250.0f;
									break;
								}
							}
						}
					}
				}
			}

			// 更新 Boss 子弹
			for (int i = 0; i < MAX_BOSS_BULLETS; i++) 
			{
				if (bossBullets[i].active) 
				{
					bossBullets[i].pos.x += bossBullets[i].vel.x * dt;
					bossBullets[i].pos.y += bossBullets[i].vel.y * dt;
					if (bossBullets[i].pos.y > 800) bossBullets[i].active = false;

					// 检测 Boss 子弹打中玩家
					if (invincibleTimer <= 0.0f && CheckCollisionCircles(bossBullets[i].pos, 6, player, 15)) 
					{
						bossBullets[i].active = false;
						playerHP -= 10;
						invincibleTimer = 1.0f;
						if (playerHP <= 0) { playerHP = 0; gameover = true; }
					}
				}
			}

			// 8. 碰撞：子弹 vs 敌机
			for (int i = 0; i < MAX_BULLETS; i++) 
			{
				if (!bullets[i].active) continue;
				for (int j = 0; j < MAX_ENEMIES; j++) 
				{
					if (!enemies[j].active) continue;

					float enemyScale = 0.4f;
					float scaledWidth = enemyTex.width * enemyScale;
					float scaledHeight = enemyTex.height * enemyScale;
					Rectangle enemyRect = 
					{
						enemies[j].pos.x - scaledWidth / 2.0f,
						enemies[j].pos.y - scaledHeight / 2.0f,
						scaledWidth,
						scaledHeight
					};

					if (CheckCollisionCircleRec(bullets[i].pos, 4, enemyRect)) 
					{
						bullets[i].active = false;
						enemies[j].active = false;
						PlaySound(explosionSound);
						score += 10;
						difficultyScore += 10; // 👈 加上这句，打小怪难度才增加
						// 掉落道具逻辑
						if (GetRandomValue(1, 100) <= 25) 
						{ // 25% 概率掉落
							for (int m = 0; m < MAX_ITEMS; m++) 
							{
								if (!items[m].active) 
								{
									items[m].active = true;
									items[m].pos = enemies[j].pos; // 掉落在敌机死的地方
									items[m].speed = 100.0f;       // 道具下落速度
									items[m].type = GetRandomValue(1, 2); // 随机回血或火力
									break;
								}
							}
						}

						// 生成爆炸粒子
						for (int p = 0; p < 20; p++) 
						{
							for (int k = 0; k < MAX_PARTICLES; k++) 
							{
								if (particles[k].life <= 0.0f) 
								{
									particles[k].pos = enemies[j].pos;
									particles[k].vel.x = (float)GetRandomValue(-200, 200);
									particles[k].vel.y = (float)GetRandomValue(-200, 200);
									particles[k].maxLife = (float)GetRandomValue(3, 6) / 10.0f;
									particles[k].life = particles[k].maxLife;
									particles[k].size = (float)GetRandomValue(2, 5);
									int colorType = GetRandomValue(1, 3);
									if (colorType == 1) particles[k].color = RED;
									else if (colorType == 2) particles[k].color = ORANGE;
									else particles[k].color = YELLOW;
									break;
								}
							}
						}
						break;
					}
				}
			}
			// 👇 玩家子弹打 Boss 👇
			if (myBoss.active) 
			{
				for (int i = 0; i < MAX_BULLETS; i++) 
				{
					if (bullets[i].active) 
					{
						// 假设 Boss 是个 100x100 的矩形
						Rectangle bossRect = { myBoss.pos.x - 50, myBoss.pos.y - 50, 100, 100 };
						if (CheckCollisionCircleRec(bullets[i].pos, 4, bossRect)) 
						{
							bullets[i].active = false;
							myBoss.hp -= 10; // 每发子弹扣 10 血

							// 可以在 Boss 中弹时生成几个小火花
							for (int p = 0; p < 5; p++) 
							{
								for (int k = 0; k < MAX_PARTICLES; k++) 
								{
									if (particles[k].life <= 0.0f) 
									{
										particles[k].pos = bullets[i].pos;
										particles[k].vel.x = (float)GetRandomValue(-100, 100);
										particles[k].vel.y = (float)GetRandomValue(-100, 100);
										particles[k].maxLife = 0.3f;
										particles[k].life = 0.3f;
										particles[k].size = 3;
										particles[k].color = RED;
										break;
									}
								}
							}

							// Boss 血量归零，爆炸！
							if (myBoss.hp <= 0) 
							{
								myBoss.active = false;
								isBossStage = false; // 恢复刷小怪
								score += 1000;       // 巨额奖励
								PlaySound(explosionSound);
								// 👇 下一次 Boss 需要比当前分数再多 1500 分才会出来！
								nextBossScore = score + 1500;
								difficultyScore = 0; // 👈 难度分数清零，重新开始计算！
								playerHP = 100;
								// 触发一场大爆炸
								for (int p = 0; p < 100; p++) 
								{
									for (int k = 0; k < MAX_PARTICLES; k++) 
									{
										if (particles[k].life <= 0.0f) 
										{
											particles[k].pos = myBoss.pos;
											particles[k].vel.x = (float)GetRandomValue(-300, 300);
											particles[k].vel.y = (float)GetRandomValue(-300, 300);
											particles[k].maxLife = (float)GetRandomValue(5, 10) / 10.0f;
											particles[k].life = particles[k].maxLife;
											particles[k].size = (float)GetRandomValue(3, 8);
											particles[k].color = YELLOW;
											break;
										}
									}
								}
							}
						}
					}
				}
			}

			// 9. 碰撞：玩家 vs 敌机
			if (invincibleTimer <= 0.0f) 
			{
				for (int i = 0; i < MAX_ENEMIES; i++) 
				{
					if (enemies[i].active) 
					{
						float enemyScale = 0.4f;
						float scaledWidth = enemyTex.width * enemyScale;
						float scaledHeight = enemyTex.height * enemyScale;
						Rectangle enemyRect = 
						{
							enemies[i].pos.x - scaledWidth / 2.0f,
							enemies[i].pos.y - scaledHeight / 2.0f,
							scaledWidth,
							scaledHeight
						};

						if (CheckCollisionCircleRec(player, 15.0f, enemyRect)) 
						{
							enemies[i].active = false;
							playerHP -= 20;
							invincibleTimer = 1.0f;
							if (playerHP <= 0) 
							{
								playerHP = 0;
								gameover = true;
							}
							break;
						}
					}
				}
			} // 👈 玩家碰撞结束

			// 10. 更新爆炸粒子
			for (int i = 0; i < MAX_PARTICLES; i++) 
			{
				if (particles[i].life > 0.0f) 
				{
					particles[i].life -= dt;
					particles[i].pos.x += particles[i].vel.x * dt;
					particles[i].pos.y += particles[i].vel.y * dt;
					particles[i].vel.x *= 0.95f;
					particles[i].vel.y *= 0.95f;
				}
			}

		} // 👈 逻辑更新区结束 (if (!gameover) 的括号)


		// ==================== 绘制区 ====================
		BeginDrawing();
		ClearBackground(Color{ 10, 10, 30, 255 });

		// 画星空
		for (int i = 0; i < MAX_STARS; i++) 
		{
			DrawCircleV(stars[i], starSizes[i], Fade(WHITE, 0.8f));
		}

		// 画子弹
		for (int i = 0; i < MAX_BULLETS; i++) 
		{
			if (bullets[i].active) 
			{
				Vector2 bPos = { bullets[i].pos.x - bulletTex.width / 2.0f, bullets[i].pos.y - bulletTex.height / 2.0f };
				DrawTextureV(bulletTex, bPos, WHITE);
			}
		}

		// 画敌机
		float enemyScale = 0.4f;
		for (int i = 0; i < MAX_ENEMIES; i++)
		{
			if (enemies[i].active) {
				float scaledWidth = enemyTex.width * enemyScale;
				float scaledHeight = enemyTex.height * enemyScale;
				Vector2 enemyDrawPos = { enemies[i].pos.x - scaledWidth / 2.0f, enemies[i].pos.y - scaledHeight / 2.0f };
				DrawTextureEx(enemyTex, enemyDrawPos, 0.0f, enemyScale, WHITE);
			}
		}
		// 画道具
		for (int i = 0; i < MAX_ITEMS; i++) 
		{
			if (items[i].active) 
			{
				if (items[i].type == 1) 
				{
					// 回血道具：绿色方块
					DrawRectangleV(items[i].pos, Vector2{ 20, 20 }, GREEN);
				}
				else if (items[i].type == 2) 
				{
					// 火力道具：黄色圆圈
					DrawCircleV(items[i].pos, 10.0f, YELLOW);
				}
			}
		}
		// 👇 画 Boss 👇
		if (myBoss.active) 
		{
			// 假设我们要让 Boss 的图片大小缩放到 150x150
			float bossScale = 1.0f; // 如果图片本身很大，把这个值改小（比如 0.5f）
			float bossWidth = bossTex.width * bossScale;
			float bossHeight = bossTex.height * bossScale;

			// 计算出居中的绘制位置
			Vector2 bossDrawPos = { myBoss.pos.x - bossWidth / 2.0f, myBoss.pos.y - bossHeight / 2.0f };

			// 使用 DrawTextureEx 绘制
			DrawTextureEx(bossTex, bossDrawPos, 0.0f, bossScale, WHITE);

			// 画 Boss 血条 (屏幕最上方)
			DrawRectangle(50, 70, 380, 20, DARKGRAY); // 血条底框
			// 计算当前血量比例
			float hpRatio = (float)myBoss.hp / myBoss.maxHp;
			DrawRectangle(50, 70, (int)(380 * hpRatio), 20, MAROON); // 红色血条
			DrawText("BOSS", 50, 45, 20, RED);
		}

		// 画 Boss 子弹
		for (int i = 0; i < MAX_BOSS_BULLETS; i++) 
		{
			if (bossBullets[i].active) 
			{
				DrawCircleV(bossBullets[i].pos, 8, PURPLE); // 紫色弹幕
			}
		}

		// 画玩家
		Vector2 drawPos = { player.x - playerTex.width / 2.0f, player.y - playerTex.height / 2.0f };
		Color playerColor = (invincibleTimer > 0 && (int)(invincibleTimer * 10) % 2 == 0) ? Fade(WHITE, 0.3f) : WHITE;
		DrawTextureV(playerTex, drawPos, playerColor);

		// 画爆炸粒子
		for (int i = 0; i < MAX_PARTICLES; i++) 
		{
			if (particles[i].life > 0.0f) 
			{
				float alpha = particles[i].life / particles[i].maxLife;
				Color pColor = Fade(particles[i].color, alpha);
				DrawCircleV(particles[i].pos, particles[i].size, pColor);
			}
		}

		// 画UI
		DrawText(TextFormat("Score: %d", score), 10, 10, 20, WHITE);
		DrawText(TextFormat("HP: %d", playerHP), 10, 40, 20, GREEN);

		// 游戏结束画面
		if (gameover) {
			DrawRectangle(0, 0, 480, 800, Fade(BLACK, 0.7f));
			DrawText("GAME OVER", 120, 300, 40, RED);
			DrawText(TextFormat("Final Score: %d", score), 140, 380, 20, WHITE);
			DrawText("Press ENTER to Restart", 120, 450, 20, LIGHTGRAY);
		}

		// 按回车重启
		if (gameover && IsKeyPressed(KEY_ENTER)) 
		{
			playerHP = 100;
			score = 0;
			difficultyScore = 0; //
			nextBossScore = 500; // 重新从 500 分开始算
			player.x = 240;
			player.y = 700;
			gameover = false;

			for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
			for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
			for (int i = 0; i < MAX_ITEMS; i++) items[i].active = false; // 清空屏幕上的道具
			powerUpTimer = 0.0f; // 重置火力升级时间
			// 👇👇👇 加上这几行彻底重置 Boss 状态 👇👇👇
			myBoss.active = false;
			isBossStage = false;
			for (int i = 0; i < MAX_BOSS_BULLETS; i++) bossBullets[i].active = false; // 清空 Boss 子弹
			powerUpTimer = 0.0f; // 重置火力时间
		}

		EndDrawing();
	} // 👈 while 循环结束


	// ==================== 清理区 ====================
	UnloadTexture(playerTex);
	UnloadTexture(enemyTex);
	UnloadTexture(bulletTex);
	UnloadSound(shootSound);
	UnloadSound(explosionSound);
	UnloadTexture(bossTex);
	CloseAudioDevice();
	CloseWindow();

	return 0;
} // 👈 main 函数结束