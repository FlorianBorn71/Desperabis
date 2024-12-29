#include "pch.h"
#include <GamePlay/GameCodeImpl.h>

namespace Decompiled
{
	/*

void GameplayTick(uint param_1)

{
	int* piVar1;
	char* pcVar2;
	sqword sVar3;
	undefined4 uVar4;
	long lVar5;
	byte bVar6;
	undefined uVar7;
	uint uVar8;
	uint uVar9;
	uint uVar10;
	ulong in_EAX;
	ulong uVar11;
	int iVar12;
	undefined2 uVar13;
	uint uVar14;
	uint uVar15;
	uint uVar16;
	ulong in_EDX;
	int iVar17;
	int iVar18;
	undefined2 uVar19;
	undefined unaff_SS;
	undefined2 unaff_DS;
	undefined uVar20;
	undefined uVar21;
	char cVar22;
	undefined uVar23;
	undefined uVar24;
	undefined uVar26;
	char cVar27;
	char cVar28;
	byte bStack69;
	byte* pbVar29;
	uint local_16;
	uint local_14;
	uint local_12;
	uint local_e;
	int local_a;
	uint local_6;
	undefined2 local_4;
	undefined uVar25;

	*(undefined*)0xd4ff = 0;
	local_6 = 2;

	// this is game code!
	// flborn: uVar9 is enum class BehaviorType
	while (true) {
		uVar19 = (undefined2)in_EDX;
		uVar9 = (uint)in_EAX;
		iVar17 = local_6 * 0x36;
		pbVar29 = (byte*)CONCAT22(unaff_DS, iVar17 + -0x7dc4);
		if (0 < *(int*)(iVar17 + -0x7dba)) {
			FUN_20ac_202a((char)*(undefined2*)(iVar17 + -0x7db4), (char)*(undefined2*)(iVar17 + -0x7db8))
				;
			*(uint*)0x5d2 = uVar9;
			FUN_2e48_d001((char)*(undefined2*)0x5d2);
			*(uint*)0x5d0 = uVar9;
			if ((0x31 < *(byte*)(iVar17 + -0x7d91)) && (*(byte*)(iVar17 + -0x7d91) < 0x79)) {
				bVar6 = *(byte*)(iVar17 + -0x7d91);
				uVar9 = (uint)bVar6;
				FUN_1000_0f95(bVar6);
				*(undefined*)(iVar17 + -0x7d91) = (char)uVar9;
			}
			cVar22 = (char)param_1;
			if ((*(char*)0xd500 != '\0') && (*(int*)(iVar17 + -0x7dba) < 1000)) {
				FUN_2e48_0a6c(cVar22, 0, (char)*(undefined2*)(iVar17 + -0x7dba));
				piVar1 = (int*)(iVar17 + -0x7dba);
				*piVar1 = *piVar1 + uVar9;
			}
			local_4 = 0;
			uVar24 = (undefined)unaff_DS;
			uVar25 = (undefined)local_6;
			if (*(char*)0x4cd != '\0') {
				uVar9 = *(uint*)(iVar17 + -0x7dbc);
				cVar28 = (char)uVar9;
				if (uVar9 == 0xffff) {
					*(undefined2*)(iVar17 + -0x7dba) = 0;
					local_4 = 0;
				}
				else {
					if (uVar9 == 1) {
						local_4 = 1;
						if (0 < (int)param_1) {
							local_4 = 1;
							uVar9 = param_1;
							while (FUN_20ac_2a2e(uVar25), local_4 != uVar9) {
								local_4 = local_4 + 1;
							}
						}
					}
					else {
						if (uVar9 == 2) {
							FUN_2e48_2b33(uVar25);
							local_4 = uVar9;
							if (1 < (int)uVar9) {
								*(undefined2*)(iVar17 + -0x7dba) = 0;
								cVar22 = *(char*)(uVar9 * 0x36 + -0x7dbe);
								uVar7 = (undefined)uVar9;
								if (cVar22 == '<') {
									local_4 = (int)(uint) * (byte*)(iVar17 + -0x7d90) >> 1;
									FUN_20ac_10f5((char)local_4, uVar7);
								}
								else {
									if ((cVar22 == '\x1e') || (cVar22 == '#')) {
										FUN_20ac_07f2((char)*(undefined2*)(uVar9 * 0x36 + -0x7db4),
											(char)*(undefined2*)(uVar9 * 0x36 + -0x7db8), 2, uVar25);
										bVar6 = *(byte*)(iVar17 + -0x7d90);
										local_4 = (uint)bVar6;
										FUN_20ac_10f5(bVar6, uVar7);
									}
									else {
										if ((cVar22 == '&') || (cVar22 == '$')) {
											FUN_20ac_07f2((char)*(undefined2*)(uVar9 * 0x36 + -0x7db4),
												(char)*(undefined2*)(uVar9 * 0x36 + -0x7db8), 4, uVar25);
											bVar6 = *(byte*)(iVar17 + -0x7d90);
											local_4 = (uint)bVar6;
											FUN_20ac_10f5(bVar6, uVar7);
										}
										else {
											if ((cVar22 == '~') || (cVar22 == '\x1a')) {
												FUN_20ac_07f2((char)*(undefined2*)(uVar9 * 0x36 + -0x7db4),
													(char)*(undefined2*)(uVar9 * 0x36 + -0x7db8), 3, uVar25);
												bVar6 = *(byte*)(iVar17 + -0x7d90);
												local_4 = (uint)bVar6;
												FUN_20ac_10f5(bVar6, uVar7);
											}
											else {
												bVar6 = *(byte*)(iVar17 + -0x7d90);
												local_4 = (uint)bVar6;
												FUN_20ac_10f5(bVar6, uVar7);
											}
										}
									}
								}
							}
						}
						else {
							if (uVar9 == 3) {
								local_4 = 1;
								if (0 < (int)param_1) {
									local_4 = 1;
									uVar9 = param_1;
									while (FUN_20ac_24a7(uVar25), local_4 != uVar9) {
										local_4 = local_4 + 1;
									}
								}
							}
							else {
								if (uVar9 == 4) {
									if (*(int*)(iVar17 + -0x7d98) < 0xc) {
										piVar1 = (int*)(iVar17 + -0x7d98);
										*piVar1 = *piVar1 + param_1;
										if (0xb < *(int*)(iVar17 + -0x7d98)) {
											uVar7 = 0x22;
											uVar9 = param_1;
											FUN_2e48_d001(100);
											FUN_2e48_d15c(1, uVar25, (char)uVar9, uVar7);
										}
										*(undefined2*)(iVar17 + -0x7db8) = *(undefined2*)0xd4d2;
										*(undefined2*)(iVar17 + -0x7db4) = *(undefined2*)0xd4d6;
										*(int*)(iVar17 + -0x7db6) = *(int*)0xd4d4 + -0x14;
										*(int*)(iVar17 + -0x7db2) = *(int*)0x8284 >> 2;
										local_4 = *(int*)0x8288 >> 2;
										*(uint*)(iVar17 + -0x7dae) = local_4;
									}
									else {
										local_4 = 1;
										if (0 < (int)param_1) {
											local_4 = 1;
											uVar9 = param_1;
											while (FUN_20ac_2182(uVar25), local_4 != uVar9) {
												local_4 = local_4 + 1;
											}
										}
									}
								}
								else {
									if (uVar9 == 5) {
										local_4 = *(int*)(iVar17 + -0x7dba) >> 2;
										*(uint*)(iVar17 + -0x7d9a) = local_4;
									}
									else {
										if (uVar9 == 6) {
											if (*(int*)(iVar17 + -0x7d9e) < 1) {
												*(undefined2*)(iVar17 + -0x7d9e) = *(undefined2*)(iVar17 + -0x7d9a);
												uVar9 = *(uint*)(iVar17 + -0x7da2);
												uVar8 = (int)uVar9 >> 0xf;
												if ((uVar9 ^ uVar8) - uVar8 != 1) {
													*(undefined2*)(iVar17 + -0x7da2) = 1;
												}
												*(char*)(iVar17 + -0x7dc1) =
													*(char*)(iVar17 + -0x7dc1) + (char)*(undefined2*)(iVar17 + -0x7da2);
												if ((*(char*)(iVar17 + -0x7dc1) == '\x01') ||
													(bVar6 = *(byte*)((uint)*pbVar29 * 0xf2 + 0x69c9), local_4 = (uint)bVar6
														, bVar6 == *(byte*)(iVar17 + -0x7dc1))) {
													local_4 = -*(int*)(iVar17 + -0x7da2);
													*(uint*)(iVar17 + -0x7da2) = local_4;
												}
											}
											else {
												piVar1 = (int*)(iVar17 + -0x7d9e);
												*piVar1 = *piVar1 - param_1;
												local_4 = param_1;
											}
										}
										else {
											local_4 = param_1;
											if (uVar9 == 7) {
												if (*(int*)(iVar17 + -0x7d9e) < 1) {
													*(undefined2*)(iVar17 + -0x7d9e) = *(undefined2*)(iVar17 + -0x7d9a);
													local_4 = (int)((ulong)(long)(int)(uint) * (byte*)(iVar17 + -0x7dc1) %
														(ulong) * (byte*)((uint)*pbVar29 * 0xf2 + 0x69c9)) + 1;
													*(undefined*)(iVar17 + -0x7dc1) = (char)local_4;
												}
												else {
													piVar1 = (int*)(iVar17 + -0x7d9e);
													*piVar1 = *piVar1 - param_1;
												}
											}
											else {
												if (uVar9 == 8) {
													FUN_20ac_387b(0x38, (char)*(undefined2*)(iVar17 + -0x7db4),
														(char)*(undefined2*)(iVar17 + -0x7db8));
													local_4 = uVar9;
												}
												else {
													iVar18 = (int)param_1 >> 0xf;
													if (uVar9 == 9) {
														iVar12 = *(int*)0x5d2;
														if ((-1 < iVar12) && (iVar12 < 0x12d)) {
															FUN_3ce4_170a(0xff);
															piVar1 = (int*)0xdc34;
															*piVar1 = *piVar1 + iVar12;
														}
														FUN_3ce4_170a(0x1e);
														uVar9 = iVar12 + 5;
														FUN_20ac_0000((char)uVar9, (char)*(undefined2*)(iVar17 + -0x7db4),
															(char)*(undefined2*)(iVar17 + -0x7db8));
														FUN_3ce4_170a(100);
														if (((int)param_1 < 0 && -1 < iVar18) ||
															((-1 < iVar18 && (uVar9 < param_1)))) {
															bVar6 = *(byte*)(iVar17 + -0x7dbf);
															local_4 = (uint)bVar6;
															FUN_2e48_d15c(2, uVar25, (char)*(undefined2*)0x5d0, bVar6);
														}
													}
													else {
														if (uVar9 == 10) {
															FUN_20ac_3f56(cVar22, uVar25);
															local_4 = uVar9;
														}
														else {
															if (uVar9 == 0xb) {
																FUN_2e48_1b45((char)*(undefined2*)(iVar17 + -0x7db4),
																	(char)*(undefined2*)(iVar17 + -0x7db6),
																	(char)*(undefined2*)(iVar17 + -0x7db8));
																*(uint*)(iVar17 + -0x7db6) = uVar9;
																*(undefined*)(iVar17 + -0x7dc1) = 3;
																*(undefined*)(iVar17 + -0x7dc2) = 0;
																FUN_2e48_0a25((char)*(undefined2*)(iVar17 + -0x7db2));
																iVar18 = uVar9 * param_1;
																piVar1 = (int*)(iVar17 + -0x7db8);
																*piVar1 = *piVar1 + iVar18;
																FUN_2e48_0a25((char)*(undefined2*)(iVar17 + -0x7dae));
																bVar6 = (byte)(iVar18 * param_1);
																piVar1 = (int*)(iVar17 + -0x7db4);
																*piVar1 = *piVar1 + iVar18 * param_1;
																FUN_2e48_cf49((char)*(undefined2*)(iVar17 + -0x7db4),
																	(char)*(undefined2*)(iVar17 + -0x7db8));
																bVar6 = bVar6 & 7;
																uVar9 = (uint)bVar6;
																local_16 = (uint)bVar6;
																if (local_16 != 1) {
																	*(undefined*)(iVar17 + -0x7dc1) = 1;
																	*(undefined*)(iVar17 + -0x7dc2) = 2;
																	*(undefined2*)(iVar17 + -0x7dbc) = 0xc;
																}
																local_4 = (uint)bVar6;
																if (local_16 == 3) {
																	FUN_20ac_0767(uVar25);
																	local_4 = uVar9;
																}
															}
															else {
																if (uVar9 == 0xc) {
																	local_4 = 1;
																	if (0 < (int)param_1) {
																		local_4 = 1;
																		uVar9 = param_1;
																		while (FUN_20ac_8bc4(uVar25), local_4 != uVar9) {
																			local_4 = local_4 + 1;
																		}
																	}
																}
																else {
																	if (uVar9 == 0xd) {
																		local_4 = 1;
																		if (0 < (int)param_1) {
																			local_4 = 1;
																			uVar9 = param_1;
																			while (FUN_20ac_3058(uVar25), local_4 != uVar9) {
																				local_4 = local_4 + 1;
																			}
																		}
																	}
																	else {
																		if (uVar9 == 0xe) {
																			local_4 = 1;
																			if (0 < (int)param_1) {
																				local_4 = 1;
																				uVar9 = param_1;
																				while (FUN_20ac_3391(local_4 == 1, 0x10,
																					(char)*(undefined2*)(iVar17 + -0x7da0)
																					, (char)*(undefined2*)
																					(iVar17 + -0x7da2), uVar25),
																					local_4 != uVar9) {
																					local_4 = local_4 + 1;
																				}
																			}
																		}
																		else {
																			if (uVar9 == 0xf) {
																				local_4 = (int)(*(int*)(iVar17 + -0x7da2) * param_1 +
																					*(int*)(iVar17 + -0x7d9a) + 200) % 200;
																				*(uint*)(iVar17 + -0x7d9a) = local_4;
																			}
																			else {
																				if (uVar9 == 0x10) {
																					FUN_20ac_c9ec(cVar22, uVar25);
																					local_4 = uVar9;
																				}
																				else {
																					if (uVar9 == 0x11) {
																						local_4 = 1;
																						if (0 < (int)param_1) {
																							local_4 = 1;
																							uVar9 = param_1;
																							while (FUN_20ac_5be7(local_4 == 1,
																								(char)*(undefined2*)
																								(iVar17 + -0x7da2), uVar25)
																								, local_4 != uVar9) {
																								local_4 = local_4 + 1;
																							}
																						}
																					}
																					else {
																						if (uVar9 == 0x12) {
																							local_4 = 1;
																							if (0 < (int)param_1) {
																								local_4 = 1;
																								uVar9 = param_1;
																								while (true) {
																									piVar1 = (int*)(iVar17 + -0x7db8);
																									*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db2);
																									piVar1 = (int*)(iVar17 + -0x7db6);
																									*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db0);
																									piVar1 = (int*)(iVar17 + -0x7db4);
																									*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7dae);
																									if ((bool)(*(byte*)(iVar17 + -0x7da2) & 1)) {
																										piVar1 = (int*)(iVar17 + -0x7db0);
																										*piVar1 = *piVar1 + -1;
																									}
																									piVar1 = (int*)(iVar17 + -0x7d9a);
																									*piVar1 = *piVar1 + -5;
																									if (*(int*)(iVar17 + -0x7d9a) < 0) {
																										*(undefined2*)(iVar17 + -0x7dba) = 0;
																									}
																									*(uint*)(iVar17 + -0x7da2) =
																										*(uint*)(iVar17 + -0x7da2) ^ 1;
																									local_16 = 0x25 - (*(int*)(iVar17 + -0x7d9a) >> 3
																										);
																									FUN_2e48_00b5(0x24, 0x1e, 0xea, unaff_SS);
																									*(undefined*)(iVar17 + -0x7d91) =
																										(undefined)local_16;
																									if (local_4 == uVar9) break;
																									local_4 = local_4 + 1;
																								}
																							}
																						}
																						else {
																							cVar27 = (char)iVar17;
																							if (uVar9 == 0x13) {
																								local_4 = 1;
																								if (0 < (int)param_1) {
																									local_4 = 1;
																									uVar9 = 1;
																									uVar8 = param_1;
																									while (true) {
																										FUN_3ce4_170a(0x14);
																										if (uVar9 == 0) {
																											FUN_2e48_0aa0(1);
																											piVar1 = (int*)(iVar17 + -0x7da2);
																											*piVar1 = *piVar1 + uVar9;
																										}
																										uVar23 = (undefined)uVar9;
																										FUN_2e48_00b5(4, 0, cVar27 + '^', uVar24);
																										uVar20 = (undefined)
																											* (undefined2*)(iVar17 + -0x7da2);
																										uVar7 = uVar25;
																										FUN_3ce4_170a(200);
																										FUN_20ac_3391(local_4 == 1, 0x2d, uVar23, uVar20,
																											uVar7);
																										if (local_4 == uVar8) break;
																										uVar9 = local_4;
																										local_4 = local_4 + 1;
																									}
																								}
																							}
																							else {
																								if (uVar9 == 0x15) {
																									FUN_20ac_384e((char)*(undefined2*)
																										(iVar17 + -0x7db0) * cVar22,
																										cVar27 + '^', uVar24, cVar27 + 'J',
																										uVar24);
																									uVar23 = (undefined)
																										* (undefined2*)(iVar17 + -0x7d9a);
																									uVar7 = 0;
																									FUN_3ce4_170a(3);
																									uVar9 = param_1;
																									FUN_3ce4_0e1c(uVar7, uVar23);
																									FUN_2e48_0a6c((char)uVar9, uVar7, uVar23);
																									piVar1 = (int*)(iVar17 + -0x7d9a);
																									*piVar1 = *piVar1 + uVar9;
																									piVar1 = (int*)(iVar17 + -0x7db0);
																									*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7d96) *
																										param_1;
																									piVar1 = (int*)(iVar17 + -0x7da6);
																									*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7da0) *
																										param_1;
																									local_16 = 0x25 - (*(int*)(iVar17 + -0x7dba) >> 2
																										);
																									uVar9 = local_16;
																									FUN_2e48_00b5(0x24, 0x1e, 0xea, unaff_SS);
																									*(undefined*)(iVar17 + -0x7d91) =
																										(undefined)local_16;
																									local_4 = uVar9 & 0xff00 | local_16 & 0xff;
																								}
																								else {
																									if (uVar9 == 0x16) {
																										FUN_3ce4_170a(0x14);
																										uVar8 = *(uint*)(iVar17 + -0x7d96);
																										if (((int)uVar8 < 0 && -1 < (int)uVar8 > > 0xf)
																											|| ((-1 < (int)uVar8 >> 0xf &&
																												(uVar9 <= uVar8)))) {
																											uVar23 = 0x3e;
																											uVar7 = (undefined)(*(int*)0x5d2 << 1);
																											FUN_2e48_d001(uVar7);
																											FUN_2e48_d15c(0xff, uVar25, uVar7, uVar23);
																										}
																										local_12 = 0xfffe;
																										while (true) {
																											local_14 = 0xfffe;
																											while (true) {
																												local_16 = (5 - ((local_12 ^
																													(int)local_12 >> 0xf) -
																													((int)local_12 >> 0xf))) -
																													((local_14 ^ (int)local_14 >> 0xf
																														) - ((int)local_14 >> 0xf));
																												cVar28 = (char)local_12 * '(' +
																													(char)*(undefined2*)
																													(iVar17 + -0x7db8);
																												cVar22 = (char)local_14 * '(' +
																													(char)*(undefined2*)
																													(iVar17 + -0x7db4);
																												FUN_3ce4_170a(0x1e);
																												iVar18 = local_16 * local_16;
																												FUN_3ce4_0e1c(cVar22, cVar28);
																												uVar9 = iVar18 >> 3;
																												FUN_20ac_0000((char)uVar9, cVar22, cVar28);
																												if (local_14 == 2) break;
																												local_14 = local_14 + 1;
																											}
																											if (local_12 == 2) break;
																											local_12 = local_12 + 1;
																										}
																										FUN_3ce4_170a(100);
																										local_4 = *(int*)(iVar17 + -0x7d96) * param_1;
																										if (((int)local_4 < 0 &&
																											-1 < (int)local_4 >> 0xf) ||
																											((-1 < (int)local_4 >> 0xf &&
																												(uVar9 < local_4)))) {
																											local_4 = *(int*)(iVar17 + -0x7db6) +
																												*(int*)(iVar17 + -0x7daa);
																											FUN_20ac_091c((char)*(undefined2*)
																												(iVar17 + -0x7db4),
																												(char)local_4,
																												(char)*(undefined2*)
																												(iVar17 + -0x7db8), 0,
																												uVar25);
																										}
																									}
																									else {
																										if (uVar9 == 0x17) {
																											lVar5 = (ulong) * (uint*)(iVar17 + -0x7dae) *
																												(ulong)param_1;
																											uVar19 = (undefined2)((ulong)lVar5 >> 0x10);
																											uVar9 = (int)lVar5 +
																												*(int*)(iVar17 + -0x7d9e) & 0x1ff;
																											*(uint*)(iVar17 + -0x7d9e) = uVar9;
																											FUN_3ce4_170a(0xf);
																											if (uVar9 == 0) {
																												FUN_3ce4_170a(10);
																												uVar9 = uVar9 + 8;
																												*(uint*)(iVar17 + -0x7db2) = uVar9;
																											}
																											FUN_2e48_0a6c(cVar22, (char)*(undefined2*)
																												(iVar17 + -0x7db2)
																												, (char)*(undefined2*)
																												(iVar17 + -0x7dae));
																											piVar1 = (int*)(iVar17 + -0x7dae);
																											*piVar1 = *piVar1 + uVar9;
																											uVar20 = 0;
																											uVar7 = 0xee;
																											uVar23 = 2;
																											FUN_2e48_0000((char)*(undefined2*)
																												(iVar17 + -0x7d9e));
																											sVar3 = (sqword)(long)(ulong)CONCAT12(uVar20,
																												CONCAT11(uVar23, uVar7)) *
																												(sqword)CONCAT13((char)((uint)uVar19 >> 8),
																													CONCAT12((char)uVar19, uVar9));
																											uVar19 = (undefined2)((qword)sVar3 >> 0x20);
																											iVar18 = (int)((qword)sVar3 >> 0x10) + 800;
																											*(int*)(iVar17 + -0x7d9c) = iVar18;
																											uVar20 = 0;
																											uVar7 = 0x19;
																											uVar23 = 0;
																											FUN_2e48_0000((char)*(undefined2*)
																												(iVar17 + -0x7d9e));
																											local_16 = 0x1e - (int)((ulong)CONCAT12(uVar20,
																												CONCAT11(uVar23, uVar7)) *
																												CONCAT13((char)((uint)uVar19 >> 8),
																													CONCAT12((char)uVar19, iVar18)) >> 0x10);
																											local_4 = local_16;
																											FUN_20ac_0000((char)local_16,
																												(char)*(undefined2*)
																												(iVar17 + -0x7db4),
																												(char)*(undefined2*)
																												(iVar17 + -0x7db8));
																										}
																										else {
																											if (uVar9 == 0x18) {
																												FUN_20ac_384e((char)*(undefined2*)
																													(iVar17 + -0x7db0) *
																													cVar22, cVar27 + '^', uVar24,
																													cVar27 + 'J', uVar24);
																												*(int*)(iVar17 + -0x7d9a) =
																													*(int*)(iVar17 + -0x7dba) >> 2;
																												if (5 < *(int*)(iVar17 + -0x7d9a)) {
																													*(undefined2*)(iVar17 + -0x7d9a) = 5;
																												}
																												if (*(byte*)(iVar17 + -0x7d8f) < 200) {
																													pcVar2 = (char*)(iVar17 + -0x7d8f);
																													*pcVar2 = *pcVar2 + (char)*(undefined2*)
																														(iVar17 + -0x7dae
																															) * cVar22;
																												}
																												local_4 = param_1 << 1;
																												FUN_2e48_0a6c((char)local_4, 0,
																													(char)*(undefined2*)
																													(iVar17 + -0x7db0));
																												piVar1 = (int*)(iVar17 + -0x7db0);
																												*piVar1 = *piVar1 + local_4;
																											}
																											else {
																												local_4 = uVar9;
																												if (uVar9 == 0x19) {
																													if (*(int*)(iVar17 + -0x7d9e) < 1) {
																														*(undefined2*)(iVar17 + -0x7d9e) = 3;
																														pcVar2 = (char*)(iVar17 + -0x7dc1);
																														*pcVar2 = *pcVar2 + '\x01';
																														if (5 < *(byte*)(iVar17 + -0x7dc1)) {
																															*(undefined*)(iVar17 + -0x7dc1) = 3;
																														}
																													}
																													else {
																														piVar1 = (int*)(iVar17 + -0x7d9e);
																														*piVar1 = *piVar1 - param_1;
																														local_4 = param_1;
																													}
																												}
																												else {
																													if (uVar9 == 0x1a) {
																														FUN_20ac_8f1e(cVar22, uVar25);
																														local_4 = uVar9;
																													}
																													else {
																														if (uVar9 == 0x1b) {
																															FUN_20ac_8fca(cVar22, uVar25);
																															local_4 = uVar9;
																														}
																														else {
																															if (uVar9 == 0x1c) {
																																uVar23 = (undefined)
																																	* (undefined2*)
																																	(iVar17 + -0x7db8);
																																uVar7 = (undefined)
																																	* (undefined2*)
																																	(iVar17 + -0x7db4);
																																FUN_3ce4_170a(0x32);
																																FUN_20ac_0000((char)uVar9, uVar7, uVar23
																																);
																																local_4 = uVar9;
																															}
																															else {
																																if (uVar9 == 0x1d) {
																																	local_4 = 1;
																																	if (0 < (int)param_1) {
																																		local_4 = 1;
																																		uVar9 = param_1;
																																		while (FUN_20ac_5dec((char)*(
																																			undefined2*)(iVar17 + -0x7da0),
																																			(char)*(undefined2*)(iVar17 + -0x7da2), uVar25),
																																			local_4 != uVar9) {
																																			local_4 = local_4 + 1;
																																		}
																																	}
																																}
																																else {
																																	if (uVar9 == 0x1e) {
																																		local_4 = 1;
																																		if (0 < (int)param_1) {
																																			local_4 = 1;
																																			uVar9 = param_1;
																																			while (FUN_20ac_4dab(uVar25),
																																				local_4 != uVar9) {
																																				local_4 = local_4 + 1;
																																			}
																																		}
																																	}
																																	else {
																																		if (uVar9 == 0x1f) {
																																			local_4 = 1;
																																			if (0 < (int)param_1) {
																																				local_4 = 1;
																																				uVar9 = param_1;
																																				while (FUN_20ac_7764(uVar25),
																																					local_4 != uVar9) {
																																					local_4 = local_4 + 1;
																																				}
																																			}
																																		}
																																		else {
																																			if (uVar9 == 0x21) {
																																				piVar1 = (int*)(iVar17 + -0x7d9a);
																																				*piVar1 = *piVar1 + *(int*)(iVar17 +
																																					-0x7da2) * param_1;
																																				piVar1 = (int*)(iVar17 + -0x7da2);
																																				*piVar1 = *piVar1 + param_1;
																																				local_4 = param_1;
																																				while (100 < *(int*)(iVar17 + -0x7d9a)) {
																																					piVar1 = (int*)(iVar17 + -0x7d9a);
																																					*piVar1 = *piVar1 + -100;
																																					pcVar2 = (char*)(iVar17 + -0x7dc1);
																																					*pcVar2 = *pcVar2 + '\x01';
																																					pcVar2 = (char*)(iVar17 + -0x7dc2);
																																					*pcVar2 = *pcVar2 + '\x01';
																																					*(undefined2*)(iVar17 + -0x7da2) = 3;
																																					if (*(char*)(iVar17 + -0x7dc2) == '\x04') {
																																						local_4 = 0;
																																						*(undefined2*)(iVar17 + -0x7dba) = 0;
																																					}
																																				}
																																			}
																																			else {
																																				if (uVar9 == 0x22) {
																																					FUN_2e48_cf8b(0, 0x38, (char)*(undefined2*)
																																						(iVar17 + -0x7db4)
																																						, (char)*(undefined2*)
																																						(iVar17 + -0x7db8));
																																					local_4 = uVar9;
																																					if (((char)uVar9 != '\0') &&
																																						((iVar18 = *(int*)0xd506, 0 < iVar18 ||
																																							((-1 < iVar18 && (0x14 < *(uint*)0xd504))
																																								)))) {
																																						*(undefined2*)(iVar17 + -0x7dbc) = 0x1d;
																																					}
																																				}
																																				else {
																																					if (uVar9 == 0x23) {
																																						local_4 = 1;
																																						if (0 < (int)param_1) {
																																							local_4 = 1;
																																							uVar9 = param_1;
																																							while (true) {
																																								if (*(int*)(iVar17 + -0x7dba) < 0x3e9)
																																								{
																																									FUN_20ac_5fe0(1, 1, 0x1e, 8, 3, uVar25);
																																								}
																																								if (local_4 == uVar9) break;
																																								local_4 = local_4 + 1;
																																							}
																																						}
																																					}
																																					else {
																																						if (uVar9 == 0x24) {
																																							local_4 = 1;
																																							if (0 < (int)param_1) {
																																								local_4 = 1;
																																								uVar9 = param_1;
																																								while (uVar19 = *(undefined2*)
																																									(iVar17 + -0x7db0),
																																									bVar6 = (byte)((int)uVar19 >> 0xf)
																																									, FUN_20ac_6567(10, ((byte)uVar19^
																																										bVar6) - bVar6,
																																										uVar25),
																																									local_4 != uVar9) {
																																									local_4 = local_4 + 1;
																																								}
																																							}
																																						}
																																						else {
																																							if (uVar9 == 0x25) {
																																								local_4 = 1;
																																								if (0 < (int)param_1) {
																																									local_4 = 1;
																																									uVar9 = param_1;
																																									while (FUN_20ac_6bb3(9, 2, uVar25),
																																										local_4 != uVar9) {
																																										local_4 = local_4 + 1;
																																									}
																																								}
																																							}
																																							else {
																																								if (uVar9 == 0x26) {
																																									local_4 = 1;
																																									if (0 < (int)param_1) {
																																										local_4 = 1;
																																										uVar9 = param_1;
																																										while (FUN_20ac_7149(0x17, uVar25),
																																											local_4 != uVar9) {
																																											local_4 = local_4 + 1;
																																										}
																																									}
																																								}
																																								else {
																																									if (uVar9 == 0x27) {
																																										local_4 = 1;
																																										if (0 < (int)param_1) {
																																											local_4 = 1;
																																											uVar9 = param_1;
																																											while (true) {
																																												if (*(int*)(iVar17 + -0x7dba) <
																																													0x3e9) {
																																													FUN_20ac_5fe0(0, 0, 0x1e, 0xd, 5,
																																														uVar25);
																																												}
																																												if (local_4 == uVar9) break;
																																												local_4 = local_4 + 1;
																																											}
																																										}
																																									}
																																									else {
																																										if (uVar9 == 0x28) {
																																											FUN_20ac_8a15(cVar22, uVar25);
																																											local_4 = uVar9;
																																										}
																																										else {
																																											if (uVar9 == 0x29) {
																																												local_4 = 1;
																																												if (0 < (int)param_1) {
																																													local_4 = 1;
																																													uVar9 = param_1;
																																													while (true) {
																																														if (*(int*)(iVar17 +
																																															-0x7dba) < 0x3e9) {
																																															FUN_20ac_47ba(uVar25);
																																														}
																																														if (local_4 == uVar9) break;
																																														local_4 = local_4 + 1;
																																													}
																																												}
																																											}
																																											else {
																																												if (uVar9 == 0x2a) {
																																													FUN_20ac_c432(cVar22, uVar25);
																																													local_4 = uVar9;
																																												}
																																												else {
																																													if (uVar9 == 0x2b) {
																																														local_4 = 1;
																																														if (0 < (int)param_1) {
																																															local_4 = 1;
																																															uVar9 = param_1;
																																															while (FUN_20ac_6567(0, 2, uVar25),
																																																local_4 != uVar9) {
																																																local_4 = local_4 + 1;
																																															}
																																														}
																																													}
																																													else {
																																														if (uVar9 == 0x2c) {
																																															FUN_20ac_8ad3(cVar22, uVar25);
																																															local_4 = uVar9;
																																														}
																																														else {
																																															if (uVar9 == 0x2d) {
																																																local_4 = 1;
																																																if (0 < (int)param_1) {
																																																	local_4 = 1;
																																																	uVar9 = param_1;
																																																	while (FUN_20ac_7149(0, uVar25),
																																																		local_4 != uVar9) {
																																																		local_4 = local_4 + 1;
																																																	}
																																																}
																																															}
																																															else {
																																																if (uVar9 == 0x2e) {
																																																	if (*(int*)(iVar17 + -0x7dba) == 1000
																																																		) {
																																																		local_4 = param_1 * *(int*)(iVar17
																																																			+ -0x7da2);
																																																		piVar1 = (int*)(iVar17 + -0x7d9a);
																																																		*piVar1 = *piVar1 + local_4;
																																																		if (100 < *(int*)(iVar17 + -0x7d9a)) {
																																																			*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																			local_4 = 0;
																																																		}
																																																	}
																																																}
																																																else {
																																																	if (uVar9 == 0x2f) {
																																																		if (0 < *(int*)(iVar17 + -0x7da2)) {
																																																			iVar18 = (int)(0x12 / (long)(int)param_1) +
																																																				1;
																																																			FUN_3ce4_170a((char)iVar18);
																																																			if (iVar18 == 0) {
																																																				piVar1 = (int*)(iVar17 + -0x7da2);
																																																				*piVar1 = *piVar1 + -1;
																																																			}
																																																		}
																																																		piVar1 = (int*)(iVar17 + -0x7d9e);
																																																		*piVar1 = *piVar1 + param_1 * 0xb;
																																																		iVar18 = *(int*)(iVar17 + -0x7da2);
																																																		iVar12 = iVar18 >> 0xf;
																																																		cVar22 = (char)((uint)iVar18 >> 8);
																																																		cVar28 = cVar22 >> 7;
																																																		bStack69 = cVar22 >> 0xf;
																																																		uVar7 = (undefined)iVar18;
																																																		FUN_2e48_0051((char)*(undefined2*)
																																																			(iVar17 + -0x7d9e));
																																																		local_4 = (uint)((ulong)(CONCAT13(bStack69,
																																																			CONCAT12(cVar28, CONCAT11(cVar22, uVar7))) *
																																																			CONCAT13((char)((uint)iVar12 >> 8),
																																																				CONCAT12((char)iVar12, iVar18))) >> 0x10);
																																																		*(uint*)(iVar17 + -0x7da4) = local_4;
																																																	}
																																																	else {
																																																		if (uVar9 == 0x30) {
																																																			FUN_20ac_90a1(cVar22, uVar25);
																																																			local_4 = uVar9;
																																																		}
																																																		else {
																																																			if (uVar9 == 0x31) {
																																																				if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																					local_4 = *(int*)(iVar17 + -0x7dae) *
																																																						param_1;
																																																					FUN_2e48_0a6c((char)local_4, 0,
																																																						(char)*(undefined2*)
																																																						(iVar17 + -0x7d9a));
																																																					piVar1 = (int*)(iVar17 + -0x7d9a);
																																																					*piVar1 = *piVar1 + local_4;
																																																					if (0 < *(int*)(iVar17 + -0x7d9a)) {
																																																						bVar6 = *(byte*)(iVar17 + -0x7dbf);
																																																						local_4 = (uint)bVar6;
																																																						FUN_2e48_d15c(2, uVar25, (char)*(
																																																							undefined2*)0x5d0, bVar6);
																																																					}
																																																				}
																																																			}
																																																			else {
																																																				if (uVar9 == 0x32) {
																																																					if (*(int*)(iVar17 + -0x7da0) == 0) {
																																																						*(undefined2*)(iVar17 + -0x7da0) = 10;
																																																					}
																																																					local_4 = *(int*)(iVar17 + -0x7d9e) * param_1
																																																						;
																																																					piVar1 = (int*)(iVar17 + -0x7d9a);
																																																					*piVar1 = *piVar1 + local_4;
																																																					if (*(int*)(iVar17 + -0x7d9a) < 0) {
																																																						*(undefined2*)(iVar17 + -0x7d9e) = 0;
																																																						local_4 = 0;
																																																						*(undefined2*)(iVar17 + -0x7d9a) = 0;
																																																					}
																																																					if (100 < *(int*)(iVar17 + -0x7d9a)) {
																																																						local_4 = -*(int*)(iVar17 + -0x7da0);
																																																						*(uint*)(iVar17 + -0x7d9e) = local_4;
																																																						*(undefined2*)(iVar17 + -0x7d9a) = 100;
																																																					}
																																																					if ((*(int*)(iVar17 + -0x7d9a) == 100) &&
																																																						(*(char*)(iVar17 + -0x7d90) != '\0')) {
																																																						*(undefined2*)(iVar17 + -0x7d9e) = 0;
																																																						local_4 = 0;
																																																					}
																																																				}
																																																				else {
																																																					if (uVar9 == 0x33) {
																																																						FUN_20ac_caec(cVar22, uVar25);
																																																						local_4 = uVar9;
																																																					}
																																																					else {
																																																						if (uVar9 == 0x34) {
																																																							lVar5 = (ulong) * (uint*)(iVar17 + -0x7db2)
																																																								* (ulong)param_1;
																																																							uVar19 = (undefined2)
																																																								((ulong)lVar5 >> 0x10);
																																																							uVar9 = (int)lVar5 +
																																																								*(int*)(iVar17 + -0x7dae) & 0x1ff
																																																								;
																																																							*(uint*)(iVar17 + -0x7dae) = uVar9;
																																																							FUN_2e48_0051((char)*(undefined2*)
																																																								(iVar17 + -0x7dae));
																																																							bStack69 = (byte)((uint)uVar19 >> 8);
																																																							*(int*)(iVar17 + -0x7d9a) =
																																																								(int)((ulong)(CONCAT13(bStack69,
																																																									CONCAT12((char)uVar19, uVar9)) * 0x32) >> 0x10) +
																																																								0x32;
																																																							local_4 = local_6;
																																																							if (local_6 == *(uint*)0xdc60) {
																																																								*(undefined2*)(iVar17 + -0x7d9a) = 0;
																																																								local_4 = 0;
																																																							}
																																																						}
																																																						else {
																																																							if (uVar9 == 0x35) {
																																																								uVar9 = *(int*)(iVar17 + -0x7d9e) * param_1;
																																																								uVar8 = (int)uVar9 >> 0xf;
																																																								iVar18 = (uVar9 ^ uVar8) - uVar8;
																																																								FUN_2e48_0a6c((char)iVar18,
																																																									(char)*(undefined2*)
																																																									(iVar17 + -0x7db0),
																																																									(char)*(undefined2*)
																																																									(iVar17 + -0x7da6));
																																																								piVar1 = (int*)(iVar17 + -0x7da6);
																																																								*piVar1 = *piVar1 + iVar18;
																																																								local_4 = *(uint*)(iVar17 + -0x7da6);
																																																								if (local_4 == *(uint*)(iVar17 + -0x7db0)) {
																																																									*(undefined*)(iVar17 + -0x7dbe) = 0;
																																																									*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																									local_4 = 0;
																																																								}
																																																							}
																																																							else {
																																																								if (uVar9 == 0x36) {
																																																									lVar5 = (ulong)param_1 *
																																																										(ulong) * (uint*)(iVar17 + -0x7db2);
																																																									uVar19 = (undefined2)((ulong)lVar5 >> 0x10);
																																																									iVar18 = (int)lVar5;
																																																									piVar1 = (int*)(iVar17 + -0x7dae);
																																																									*piVar1 = *piVar1 + iVar18;
																																																									FUN_2e48_0051((char)*(undefined2*)
																																																										(iVar17 + -0x7dae));
																																																									bStack69 = (byte)((uint)uVar19 >> 8);
																																																									local_4 = (int)((ulong)(CONCAT13(bStack69,
																																																										CONCAT12((char)uVar19, iVar18)) * 0x32) >> 0x10) +
																																																										0x32;
																																																									*(uint*)(iVar17 + -0x7d9a) = local_4;
																																																									if (0x80 < *(int*)(iVar17 + -0x7dae)) {
																																																										bVar6 = *(byte*)(iVar17 + -0x7dc2);
																																																										uVar9 = (uint)bVar6;
																																																										*(byte*)(iVar17 + -0x7dc1) = bVar6;
																																																										if (*(int*)(iVar17 + -0x7da2) == 0) {
																																																											*(undefined2*)(iVar17 + -0x7da2) = 3;
																																																										}
																																																										do {
																																																											do {
																																																												uVar7 = (undefined)uVar9;
																																																												FUN_3ce4_170a(4);
																																																												*(undefined*)(iVar17 + -0x7dc2) = uVar7;
																																																												uVar9 = 1 << (*(byte*)(iVar17 + -0x7dc2) &
																																																													0x1f) &
																																																													*(uint*)(iVar17 + -0x7da2);
																																																											} while ((int)uVar9 < 1);
																																																											uVar9 = *(byte*)(iVar17 + -0x7dc2) + 1;
																																																										} while (uVar9 == *(byte*)(iVar17 + -0x7dc1));
																																																										pcVar2 = (char*)(iVar17 + -0x7dc2);
																																																										*pcVar2 = *pcVar2 + '\x01';
																																																										*(undefined2*)(iVar17 + -0x7dae) = 0xff80;
																																																										*(undefined2*)(iVar17 + -0x7d9a) = 0;
																																																										local_4 = 0;
																																																									}
																																																								}
																																																								else {
																																																									if (uVar9 == 0x37) {
																																																										if ((((*(char*)0xd4fc != '\0') &&
																																																											(0 < *(int*)(iVar17 + -0x7da2))) &&
																																																											(0 < *(int*)(iVar17 + -0x7da0))) &&
																																																											(iVar18 = *(int*)(iVar17 + -0x7da2) * 4,
																																																												local_4 = *(uint*)(iVar18 + 0x697c) |
																																																												*(uint*)(iVar18 + 0x697e),
																																																												local_4 != 0)) {
																																																											uVar9 = *(int*)(iVar17 + -0x7d9c) * param_1
																																																												;
																																																											local_4 = 1;
																																																											if (0 < (int)uVar9) {
																																																												local_4 = 1;
																																																												while (true) {
																																																													piVar1 = *(int**)(*(int*)(iVar17 +
																																																														-0x7da0) * 4 + 0x697c);
																																																													iVar12 = (int)piVar1;
																																																													iVar18 = *piVar1 + 1;
																																																													FUN_3ce4_170a((char)iVar18);
																																																													bVar6 = *(byte*)(iVar12 + iVar18 + 4);
																																																													piVar1 = *(int**)(*(int*)(iVar17 + -0x7da2) * 4
																																																														+ 0x697c);
																																																													uVar19 = (undefined2)((ulong)piVar1 >> 0x10);
																																																													iVar12 = (int)piVar1;
																																																													iVar18 = (*piVar1 + 1) *
																																																														(*(int*)(iVar12 + 2) + 1);
																																																													FUN_3ce4_170a((char)iVar18);
																																																													*(int*)(iVar17 + -0x7db2) = iVar18;
																																																													*(byte*)(iVar12 + *(int*)(iVar17 + -0x7db2) + 4)
																																																														= bVar6;
																																																													local_16 = (uint)bVar6;
																																																													if (local_4 == uVar9) break;
																																																													local_4 = local_4 + 1;
																																																												}
																																																											}
																																																										}
																																																									}
																																																									else {
																																																										if (uVar9 == 0x38) {
																																																											piVar1 = (int*)(iVar17 + -0x7da4);
																																																											*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7d9e)
																																																												* param_1;
																																																											*(undefined2*)(iVar17 + -0x7d9a) =
																																																												*(undefined2*)(iVar17 + -0x7da4);
																																																											if (*(int*)(iVar17 + -0x7dae) <=
																																																												*(int*)(iVar17 + -0x7da4)) {
																																																												*(int*)(iVar17 + -0x7d9e) =
																																																													-*(int*)(iVar17 + -0x7da0);
																																																												*(undefined2*)(iVar17 + -0x7da4) =
																																																													*(undefined2*)(iVar17 + -0x7dae);
																																																											}
																																																											if (*(int*)(iVar17 + -0x7da4) < 1) {
																																																												*(undefined2*)(iVar17 + -0x7d9e) = 0;
																																																												*(undefined2*)(iVar17 + -0x7da4) = 0;
																																																											}
																																																											local_4 = *(uint*)(iVar17 + -0x7da4);
																																																											if ((local_4 == *(uint*)(iVar17 + -0x7dae))
																																																												&& (*(char*)(iVar17 + -0x7d90) != '\0')) {
																																																												*(undefined2*)(iVar17 + -0x7d9e) = 0;
																																																												local_4 = 0;
																																																											}
																																																										}
																																																										else {
																																																											if (uVar9 == 0x39) {
																																																												if (*(int*)(iVar17 + -0x7da8) == 0) {
																																																													uVar9 = *(int*)0xd4d4 + 0x28;
																																																													FUN_2e48_18cf((char)*(undefined2*)
																																																														(iVar17 + -0x7db4),
																																																														(char)uVar9,
																																																														(char)*(undefined2*)
																																																														(iVar17 + -0x7db8));
																																																													*(uint*)(iVar17 + -0x7db6) = uVar9;
																																																												}
																																																												FUN_2e48_2b33(uVar25);
																																																												local_4 = uVar9;
																																																												if (0 < (int)uVar9) {
																																																													iVar18 = uVar9 * 0x36;
																																																													bVar6 = *(byte*)(iVar18 + -0x7dbe);
																																																													local_4 = uVar9 & 0xff00 | (uint)bVar6;
																																																													if (bVar6 == 0x39) {
																																																														uVar19 = unaff_DS;
																																																														FUN_2e48_d15c(1, uVar25, (char)*(
																																																															undefined2*)0x5d0,
																																																															*(undefined*)(local_6 * 0x36 + -0x7dbf));
																																																														*(undefined*)(iVar18 + -0x7dbe) = 4;
																																																														*(undefined*)(iVar18 + -0x7dc1) = 2;
																																																														local_4 = 0;
																																																														*(undefined2*)(local_6 * 0x36 + -0x7dba) = 0;
																																																													}
																																																													else {
																																																														if ((bVar6 == 0x7d) &&
																																																															(*(int*)(local_6 * 0x36 + -0x7d96) == 0x15))
																																																														{
																																																															FUN_20ac_10f5(0, (char)uVar9);
																																																														}
																																																													}
																																																												}
																																																												local_16 = uVar9;
																																																												if ((*(int*)(iVar17 + -0x7d96) == 0x15) &&
																																																													(FUN_2e48_cf8b(0, 7, (char)*(undefined2*)
																																																														(iVar17 + -0x7db4),
																																																														(char)*(undefined2*)
																																																														(iVar17 + -0x7db8)),
																																																														(char)local_4 == '\0')) {
																																																													*(undefined2*)(iVar17 + -0x7d96) = 0x14;
																																																													*(undefined*)(iVar17 + -0x7dc1) = 1;
																																																													FUN_2e48_2049();
																																																													FUN_20ac_044e(1, 1, 0, (char)*(undefined2*)
																																																														(iVar17 + -0x7db4),
																																																														(char)*(undefined2*)
																																																														(iVar17 + -0x7db6),
																																																														(char)*(undefined2*)
																																																														(iVar17 + -0x7db8),
																																																														(char)local_4);
																																																												}
																																																											}
																																																											else {
																																																												if (uVar9 == 0x3a) {
																																																													local_4 = *(uint*)(iVar17 + -0x7db6);
																																																													if (((((int)local_4 < 0x3c) ||
																																																														(0x50 < (int)local_4)) &&
																																																														(((int)local_4 < -0x8c ||
																																																															(-0x78 < (int)local_4)))) &&
																																																														(((int)local_4 < -0x154 ||
																																																															(-0x140 < (int)local_4)))) {
																																																														*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																													}
																																																													else {
																																																														*(undefined*)(iVar17 + -0x7dbd) = 1;
																																																													}
																																																												}
																																																												else {
																																																													if (uVar9 == 0x3c) {
																																																														local_4 = 1;
																																																														if (0 < (int)param_1) {
																																																															local_4 = 1;
																																																															uVar9 = param_1;
																																																															while (FUN_20ac_5697(0x32, uVar25),
																																																																local_4 != uVar9) {
																																																																local_4 = local_4 + 1;
																																																															}
																																																														}
																																																													}
																																																													else {
																																																														if (uVar9 == 0x3d) {
																																																															FUN_20ac_59b8(cVar22, uVar25);
																																																															local_4 = uVar9;
																																																														}
																																																														else {
																																																															if (uVar9 == 0x3e) {
																																																																FUN_20ac_5964(uVar25);
																																																																local_4 = uVar9;
																																																															}
																																																															else {
																																																																if (uVar9 == 0x3f) {
																																																																	FUN_20ac_9117(cVar22, uVar25);
																																																																	local_4 = uVar9;
																																																																}
																																																																else {
																																																																	if (uVar9 == 0x40) {
																																																																		if (*(char*)0xd500 != '\0') {
																																																																			FUN_20ac_aee0(uVar25);
																																																																			local_4 = uVar9;
																																																																		}
																																																																	}
																																																																	else {
																																																																		if (uVar9 == 0x41) {
																																																																			local_4 = 1;
																																																																			if (0 < (int)param_1) {
																																																																				local_4 = 1;
																																																																				uVar9 = param_1;
																																																																				while (FUN_20ac_38d6(uVar25),
																																																																					local_4 != uVar9) {
																																																																					local_4 = local_4 + 1;
																																																																				}
																																																																			}
																																																																		}
																																																																		else {
																																																																			if (uVar9 == 0x42) {
																																																																				local_4 = 1;
																																																																				if (0 < (int)param_1) {
																																																																					local_4 = 1;
																																																																					uVar9 = param_1;
																																																																					while (FUN_20ac_3bbd(uVar25),
																																																																						local_4 != uVar9) {
																																																																						local_4 = local_4 + 1;
																																																																					}
																																																																				}
																																																																			}
																																																																			else {
																																																																				if (uVar9 == 0x43) {
																																																																					local_4 = 1;
																																																																					if (0 < (int)param_1) {
																																																																						local_4 = 1;
																																																																						uVar9 = param_1;
																																																																						while (FUN_20ac_b6b2(uVar25)
																																																																							, local_4 != uVar9) {
																																																																							local_4 = local_4 + 1;
																																																																						}
																																																																					}
																																																																				}
																																																																				else {
																																																																					if (uVar9 == 0x44) {
																																																																						local_4 = 1;
																																																																						if (0 < (int)param_1) {
																																																																							local_4 = 1;
																																																																							uVar9 = param_1;
																																																																							while (FUN_20ac_93e9(0x37,
																																																																								0x18, 0x10, uVar25), local_4 != uVar9) {
																																																																								local_4 = local_4 + 1;
																																																																							}
																																																																						}
																																																																					}
																																																																					else {
																																																																						if (uVar9 == 0x45) {
																																																																							if (0 < *(int*)(iVar17 + -0x7da2)) {
																																																																								FUN_20ac_984b(uVar25);
																																																																								local_4 = uVar9;
																																																																							}
																																																																						}
																																																																						else {
																																																																							if (uVar9 == 0x46) {
																																																																								if ((*(int*)(iVar17 + -0x7dba) == 1000) &&
																																																																									(local_4 = 1, 0 < (int)param_1)) {
																																																																									local_4 = 1;
																																																																									uVar9 = param_1;
																																																																									while (FUN_20ac_b833(uVar25),
																																																																										local_4 != uVar9) {
																																																																										local_4 = local_4 + 1;
																																																																									}
																																																																								}
																																																																							}
																																																																							else {
																																																																								if (uVar9 == 0x47) {
																																																																									FUN_20ac_1fc7(uVar25);
																																																																									*(undefined*)(iVar17 + -0x7dc1) =
																																																																										(char)(uVar9 + 1);
																																																																									local_4 = uVar9 + 1;
																																																																								}
																																																																								else {
																																																																									if (uVar9 == 0x48) {
																																																																										local_4 = 1;
																																																																										if (0 < (int)param_1) {
																																																																											local_4 = 1;
																																																																											uVar9 = param_1;
																																																																											while (FUN_20ac_9f31(uVar25),
																																																																												local_4 != uVar9) {
																																																																												local_4 = local_4 + 1;
																																																																											}
																																																																										}
																																																																									}
																																																																									else {
																																																																										if (uVar9 == 0x49) {
																																																																											FUN_20ac_0000(0xfa, (char)*(undefined2
																																																																												*)(iVar17 +
																																																																													-0x7db4),
																																																																												(char)*(undefined2*)
																																																																												(iVar17 + -0x7db8
																																																																													));
																																																																											local_4 = uVar9;
																																																																										}
																																																																										else {
																																																																											if (uVar9 == 0x4a) {
																																																																												if (*(int*)(iVar17 + -0x7dba) ==
																																																																													1000) {
																																																																													FUN_20ac_ae19(cVar22, uVar25);
																																																																													local_4 = uVar9;
																																																																												}
																																																																											}
																																																																											else {
																																																																												if (uVar9 == 0x4c) {
																																																																													if (*(int*)(iVar17 + -0x7dba) ==
																																																																														1000) {
																																																																														*(undefined*)(iVar17 + -0x7dbe)
																																																																															= 0x4c;
																																																																														FUN_2e48_2d63(uVar25);
																																																																														if (cVar28 == '\0') {
																																																																															*(undefined2*)
																																																																																(iVar17 + -0x7db0) = 0xfffe;
																																																																														}
																																																																														else {
																																																																															*(undefined2*)
																																																																																(iVar17 + -0x7db0) = 1;
																																																																														}
																																																																														local_16 = *(int*)(iVar17 +
																																																																															-0x7db0) * param_1;
																																																																														piVar1 = (int*)(iVar17 + -0x7db6);
																																																																														*piVar1 = *piVar1 + local_16;
																																																																														FUN_2e48_18cf((char)*(undefined2*)
																																																																															(iVar17 + -0x7db4),
																																																																															(char)*(undefined2*)
																																																																															(iVar17 + -0x7db6),
																																																																															(char)*(undefined2*)
																																																																															(iVar17 + -0x7db8));
																																																																														if (*(int*)(iVar17 + -0x7db6) < (int)local_16) {
																																																																															*(uint*)(iVar17 + -0x7db6) = local_16;
																																																																														}
																																																																														local_4 = *(uint*)(iVar17 + -0x7db6);
																																																																														if (local_4 != local_16) {
																																																																															bVar6 = *(byte*)(iVar17 + -0x7dbf);
																																																																															local_4 = (uint)bVar6;
																																																																															FUN_2e48_d15c(1, uVar25, (char)*(undefined2*)
																																																																																0x5d0, bVar6);
																																																																														}
																																																																													}
																																																																												}
																																																																												else {
																																																																													if (uVar9 == 0x4d) {
																																																																														uVar9 = *(int*)0x5d2 - 100;
																																																																														local_4 = (int)uVar9 >> 0xf;
																																																																														local_4 = (uVar9 ^ local_4) - local_4;
																																																																														if ((((int)local_4 < 0x15) &&
																																																																															(*(char*)0xddb6 == '\a')) &&
																																																																															(*(int*)0xdd44 < 0x168)) {
																																																																															local_4 = *(int*)0xd4ca * 2 + 2;
																																																																															piVar1 = (int*)0xdd44;
																																																																															*piVar1 = *piVar1 + local_4;
																																																																															FUN_1000_a8f7();
																																																																														}
																																																																													}
																																																																													else {
																																																																														if (uVar9 == 0x4e) {
																																																																															if (0 < *(int*)(iVar17 + -0x7d96)) {
																																																																																*(uint*)0x5cc = local_6;
																																																																																local_4 = local_6;
																																																																															}
																																																																														}
																																																																														else {
																																																																															if (uVar9 == 0x4f) {
																																																																																if (*(int*)(iVar17 + -0x7da2) < 1) {
																																																																																	*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																	uVar7 = *(undefined*)(iVar17 + -0x7dbf)
																																																																																		;
																																																																																	uVar23 = uVar7;
																																																																																	FUN_2e48_d001(0);
																																																																																	FUN_2e48_d15c(1, uVar25, uVar23, uVar7);
																																																																																	local_4 = param_1 * 0x11;
																																																																																	FUN_2e48_0a6c((char)local_4, 100,
																																																																																		(char)*(undefined2*)
																																																																																		(iVar17 + -0x7d9a))
																																																																																		;
																																																																																	piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																	*piVar1 = *piVar1 + local_4;
																																																																																	if (99 < *(int*)(iVar17 + -0x7d9a)) {
																																																																																		*(undefined*)(iVar17 + -0x7dc1) =
																																																																																			*(undefined*)(iVar17 + -0x7dc2);
																																																																																		*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																		local_4 = 0;
																																																																																	}
																																																																																}
																																																																																else {
																																																																																	if ((*(char*)0x4cd != '\0') &&
																																																																																		(FUN_2e48_2d63(uVar25),
																																																																																			local_4 = uVar9, (char)uVar9 != '\0')
																																																																																		) {
																																																																																		piVar1 = (int*)(iVar17 + -0x7da2);
																																																																																		*piVar1 = *piVar1 - param_1;
																																																																																		local_4 = param_1;
																																																																																	}
																																																																																}
																																																																															}
																																																																															else {
																																																																																if (uVar9 == 0x50) {
																																																																																	local_4 = 1;
																																																																																	if (0 < (int)param_1) {
																																																																																		local_4 = 1;
																																																																																		uVar9 = param_1;
																																																																																		while (FUN_20ac_8dd8(uVar25),
																																																																																			local_4 != uVar9) {
																																																																																			local_4 = local_4 + 1;
																																																																																		}
																																																																																	}
																																																																																}
																																																																																else {
																																																																																	if (uVar9 == 0x51) {
																																																																																		local_4 = 1;
																																																																																		if (0 < (int)param_1) {
																																																																																			local_4 = 1;
																																																																																			uVar9 = param_1;
																																																																																			while (true) {
																																																																																				if (*(int*)(iVar17 + -0x7da0) ==
																																																																																					*(int*)(iVar17 + -0x7d98)) {
																																																																																					FUN_2e48_d15c(1, uVar25, (char)*(
																																																																																						undefined2*)0x5d0, 0x16);
																																																																																				}
																																																																																				if (((bool)(*(byte*)(iVar17 + -0x7da0) & 1)) &&
																																																																																					(*(int*)(iVar17 + -0x7da0) <=
																																																																																						*(int*)(iVar17 + -0x7d98))) {
																																																																																					FUN_20ac_4106(*(undefined*)(iVar17 + -0x7d90),
																																																																																						*(undefined2*)(iVar17 + -0x7da6),
																																																																																						(char)*(undefined2*)
																																																																																						(iVar17 + -0x7db4),
																																																																																						(char)*(undefined2*)
																																																																																						(iVar17 + -0x7db6),
																																																																																						(char)*(undefined2*)
																																																																																						(iVar17 + -0x7db8), 7,
																																																																																						(char)local_4 + -1);
																																																																																				}
																																																																																				if (*(int*)(iVar17 + -0x7da0) < 1) {
																																																																																					*(undefined2*)(iVar17 + -0x7da0) =
																																																																																						*(undefined2*)(iVar17 + -0x7da2);
																																																																																				}
																																																																																				else {
																																																																																					piVar1 = (int*)(iVar17 + -0x7da0);
																																																																																					*piVar1 = *piVar1 + -1;
																																																																																				}
																																																																																				if (local_4 == uVar9) break;
																																																																																				local_4 = local_4 + 1;
																																																																																			}
																																																																																		}
																																																																																	}
																																																																																	else {
																																																																																		if (uVar9 == 0x52) {
																																																																																			lVar5 = (ulong) * (uint*)(iVar17 + -0x7dae) *
																																																																																				(ulong)param_1;
																																																																																			uVar19 = (undefined2)((ulong)lVar5 >> 0x10);
																																																																																			uVar9 = (int)lVar5 +
																																																																																				*(int*)(iVar17 + -0x7d9e) & 0x1ff;
																																																																																			*(uint*)(iVar17 + -0x7d9e) = uVar9;
																																																																																			uVar20 = 0;
																																																																																			uVar7 = 5;
																																																																																			uVar23 = 0;
																																																																																			FUN_2e48_0000((char)*(undefined2*)
																																																																																				(iVar17 + -0x7d9e));
																																																																																			local_16 = (uint)((ulong)CONCAT12(uVar20,
																																																																																				CONCAT11(uVar23, uVar7)) *
																																																																																				CONCAT13((char)((uint)uVar19 >> 8),
																																																																																					CONCAT12((char)uVar19, uVar9)) >> 0x10);
																																																																																			local_4 = local_16 + 4;
																																																																																			*(uint*)(iVar17 + -0x7d9a) = local_4;
																																																																																		}
																																																																																		else {
																																																																																			if (uVar9 == 0x53) {
																																																																																				if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																					*(undefined*)(iVar17 + -0x7dbd) = 3;
																																																																																					FUN_2e48_2b33(uVar25);
																																																																																					local_4 = uVar9;
																																																																																					FUN_2e48_18cf((char)*(undefined2*)
																																																																																						(iVar17 + -0x7db4),
																																																																																						(char)*(undefined2*)
																																																																																						(iVar17 + -0x7db6),
																																																																																						(char)*(undefined2*)
																																																																																						(iVar17 + -0x7db8));
																																																																																					*(uint*)(iVar17 + -0x7db6) = local_4;
																																																																																					local_16 = uVar9;
																																																																																					if ((0 < (int)uVar9) &&
																																																																																						(iVar18 = uVar9 * 0x36,
																																																																																							*(char*)(iVar18 + -0x7dbe) == 'D')) {
																																																																																						*(undefined*)(iVar18 + -0x7dc3) = 4;
																																																																																						*(undefined*)(iVar18 + -0x7dc1) = 1;
																																																																																						*(undefined*)(iVar18 + -0x7dc2) = 2;
																																																																																						*(undefined2*)(iVar18 + -0x7dbc) = 0xf;
																																																																																						*(undefined2*)(iVar18 + -0x7da2) = 0xb;
																																																																																						*(undefined*)(iVar18 + -0x7dbd) = 0;
																																																																																						iVar18 = local_6 * 0x36;
																																																																																						*(undefined*)(iVar18 + -0x7dbd) = 0;
																																																																																						*(undefined2*)(iVar18 + -0x7dbc) = 0;
																																																																																						*(undefined*)(iVar18 + -0x7dc0) = 6;
																																																																																						local_4 = 0;
																																																																																					}
																																																																																				}
																																																																																				else {
																																																																																					*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																				}
																																																																																			}
																																																																																			else {
																																																																																				if (uVar9 == 0x54) {
																																																																																					if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																						*(undefined*)(iVar17 + -0x7dbd) = 1;
																																																																																					}
																																																																																				}
																																																																																				else {
																																																																																					if (uVar9 == 0x55) {
																																																																																						if ((*(int*)(iVar17 + -0x7dba) == 1000)
																																																																																							&& (local_4 = 1, 0 < (int)param_1)) {
																																																																																							local_4 = 1;
																																																																																							uVar9 = param_1;
																																																																																							while (FUN_20ac_7da3(1, uVar25),
																																																																																								local_4 != uVar9) {
																																																																																								local_4 = local_4 + 1;
																																																																																							}
																																																																																						}
																																																																																					}
																																																																																					else {
																																																																																						if (uVar9 == 0x56) {
																																																																																							local_4 = 1;
																																																																																							if (0 < (int)param_1) {
																																																																																								local_4 = 1;
																																																																																								uVar9 = param_1;
																																																																																								while (FUN_20ac_819c(uVar25),
																																																																																									local_4 != uVar9) {
																																																																																									local_4 = local_4 + 1;
																																																																																								}
																																																																																							}
																																																																																						}
																																																																																						else {
																																																																																							if (uVar9 == 0x57) {
																																																																																								*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																								local_4 = param_1 * 5;
																																																																																								FUN_2e48_0a6c((char)local_4, 0,
																																																																																									(char)*(undefined2*)
																																																																																									(iVar17 + -0x7da8
																																																																																										));
																																																																																								piVar1 = (int*)(iVar17 + -0x7da8);
																																																																																								*piVar1 = *piVar1 + local_4;
																																																																																								uVar7 = (undefined)
																																																																																									* (undefined2*)
																																																																																									(iVar17 + -0x7db6);
																																																																																								FUN_2e48_18cf((char)*(undefined2*)
																																																																																									(iVar17 + -0x7db4
																																																																																										), (char)*(
																																																																																											undefined2*)(iVar17 + -0x7db6),
																																																																																									(char)*(undefined2*)(iVar17 + -0x7db8));
																																																																																								FUN_2e48_0a6c(cVar22, (char)local_4, uVar7);
																																																																																								piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																								*piVar1 = *piVar1 + local_4;
																																																																																								local_16 = 0;
																																																																																							}
																																																																																							else {
																																																																																								if (uVar9 == 0x58) {
																																																																																									FUN_20ac_8799(cVar22, uVar25);
																																																																																									local_4 = uVar9;
																																																																																								}
																																																																																								else {
																																																																																									if (uVar9 == 0x59) {
																																																																																										local_4 = *(uint*)0x5d2;
																																																																																										if ((int)local_4 <
																																																																																											*(int*)(iVar17 + -0x7da0)) {
																																																																																											iVar18 = *(int*)(iVar17 + -0x7da2) *
																																																																																												param_1;
																																																																																											iVar12 = iVar18 >> 0xf;
																																																																																											cVar22 = (char)((uint)iVar18 >> 8);
																																																																																											cVar28 = cVar22 >> 7;
																																																																																											cVar27 = cVar22 >> 0xf;
																																																																																											uVar23 = (undefined)iVar18;
																																																																																											uVar7 = uVar25;
																																																																																											FUN_2e48_0051((char)*(undefined2*)
																																																																																												(iVar17 + -0x7d9e));
																																																																																											iVar18 = (int)((ulong)(CONCAT13(cVar27,
																																																																																												CONCAT12(cVar28, CONCAT11(cVar22, uVar23))) *
																																																																																												CONCAT13((char)((uint)iVar12 >> 8),
																																																																																													CONCAT12((char)iVar12, iVar18))) >> 0x10);
																																																																																											FUN_3ce4_0ebd(uVar7);
																																																																																											local_4 = iVar18 + 1;
																																																																																											FUN_20ac_84aa((char)local_4, uVar7);
																																																																																										}
																																																																																									}
																																																																																									else {
																																																																																										if (uVar9 == 0x5b) {
																																																																																											FUN_20ac_bac9(cVar22, uVar25);
																																																																																											local_4 = uVar9;
																																																																																										}
																																																																																										else {
																																																																																											if (uVar9 == 0x5c) {
																																																																																												if (0 < *(int*)(iVar17 + -0x7d96)) {
																																																																																													uVar9 = param_1 * *(int*)(iVar17 +
																																																																																														-0x7da2);
																																																																																													uVar11 = (ulong)uVar9;
																																																																																													piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																													*piVar1 = *piVar1 + uVar9;
																																																																																													while (100 < *(int*)(iVar17 + -0x7d9a)) {
																																																																																														cVar22 = (char)(iVar17 + -0x7dc4);
																																																																																														FUN_20ac_10a8(iVar17 + -0x7d9a, uVar24,
																																																																																															cVar22 + '\x03', uVar24,
																																																																																															cVar22 + '\x02', uVar24);
																																																																																														if ((*(char*)(iVar17 + -0x7dc1) == '\x04') &&
																																																																																															(iVar18 = *(int*)(iVar17 + -0x7d96) * 0x36,
																																																																																																*(int*)(iVar18 + -0x7db0) == 0)) {
																																																																																															*(undefined2*)(iVar18 + -0x7db0) = 7;
																																																																																														}
																																																																																													}
																																																																																													if (*(char*)(iVar17 + -0x7dc1) == '\x04') {
																																																																																														*(undefined2*)
																																																																																															(*(int*)(iVar17 + -0x7d96) * 0x36 + -0x7da0) =
																																																																																															5;
																																																																																													}
																																																																																													local_4 = (uint)uVar11;
																																																																																													if (*(int*)(*(int*)(iVar17 + -0x7d96) * 0x36 +
																																																																																														-0x7dbc) == 0) {
																																																																																														*(undefined2*)(iVar17 + -0x7d9a) = 0xffc4;
																																																																																														*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																														local_4 = 0;
																																																																																													}
																																																																																												}
																																																																																											}
																																																																																											else {
																																																																																												if (uVar9 == 0x5d) {
																																																																																													local_4 = param_1 * *(int*)(iVar17 + -0x7da2)
																																																																																														;
																																																																																													piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																													*piVar1 = *piVar1 + local_4;
																																																																																													if (0 < *(int*)(iVar17 + -0x7d96)) {
																																																																																														local_4 = *(int*)(*(int*)(iVar17 + -0x7d96
																																																																																															) * 0x36 +
																																																																																															-0x7d9a) >> 1;
																																																																																														*(uint*)(iVar17 + -0x7da8) = local_4;
																																																																																													}
																																																																																													while (100 < *(int*)(iVar17 + -0x7d9a)) {
																																																																																														piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																														*piVar1 = *piVar1 + -100;
																																																																																														bVar6 = *(byte*)(iVar17 + -0x7dc2);
																																																																																														local_4 = local_4 & 0xff00 | (uint)bVar6;
																																																																																														*(byte*)(iVar17 + -0x7dc1) = bVar6;
																																																																																														if (0 < *(int*)(iVar17 + -0x7da0)) {
																																																																																															while ((bVar6 = *(byte*)(iVar17 + -0x7dc1
																																																																																																),
																																																																																																local_4 = (uint)bVar6,
																																																																																																bVar6 == *(byte*)(iVar17 + -0x7dc2
																																																																																																	) ||
																																																																																																(local_4 = 1 << (*(byte*)(iVar17 +
																																																																																																	-0x7dc2)
																																																																																																	& 0x1f) &
																																																																																																	*(uint*)(iVar17 +
																																																																																																		-0x7da0), local_4 == 0))) {
																																																																																																cVar22 = (char)local_4;
																																																																																																FUN_3ce4_170a(5);
																																																																																																*(char*)(iVar17 + -0x7dc2) = cVar22 + '\x01';
																																																																																															}
																																																																																														}
																																																																																													}
																																																																																													if (*(int*)(*(int*)(iVar17 + -0x7d96) * 0x36 +
																																																																																														-0x7dbc) == 0) {
																																																																																														*(undefined2*)(iVar17 + -0x7d9a) = 0xffc4;
																																																																																														*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																														local_4 = 0;
																																																																																													}
																																																																																												}
																																																																																												else {
																																																																																													if (uVar9 == 0x5e) {
																																																																																														if (0 < *(int*)(iVar17 + -0x7d96)) {
																																																																																															local_4 = *(uint*)(*(int*)(iVar17 +
																																																																																																-0x7d96) * 0x36 + -0x7d9a);
																																																																																															*(uint*)(iVar17 + -0x7d9a) = local_4;
																																																																																														}
																																																																																													}
																																																																																													else {
																																																																																														if (uVar9 == 0x5f) {
																																																																																															if (*(int*)(iVar17 + -0x7d98) < 1) {
																																																																																																*(undefined2*)(iVar17 + -0x7dbc) = 0x60;
																																																																																																*(undefined2*)(iVar17 + -0x7dba) = 0x1e;
																																																																																																*(undefined*)(iVar17 + -0x7d90) = 1;
																																																																																																*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																																FUN_2e48_d15c(1, uVar25, (char)*(undefined2*)
																																																																																																	0x5d0, 0x3c);
																																																																																															}
																																																																																															local_4 = uVar9;
																																																																																															if (*(char*)(iVar17 + -0x7d90) != '\0') {
																																																																																																*(undefined2*)(iVar17 + -0x7dac) = 0x5a;
																																																																																																*(undefined*)(iVar17 + -0x7dbd) = 0x80;
																																																																																																FUN_2e48_2b33(uVar25);
																																																																																																*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																																if ((0 < (int)uVar9) &&
																																																																																																	(*(char*)(uVar9 * 0x36 + -0x7dbe) == '/'
																																																																																																		)) {
																																																																																																	*(undefined*)(uVar9 * 0x36 + -0x7d90) = 1
																																																																																																		;
																																																																																																}
																																																																																																FUN_3ce4_170a(0x32);
																																																																																																piVar1 = (int*)(iVar17 + -0x7d98);
																																																																																																*piVar1 = *piVar1 - (uVar9 + 0x1e);
																																																																																																local_4 = uVar9 + 0x1e;
																																																																																															}
																																																																																														}
																																																																																														else {
																																																																																															if (uVar9 == 0x60) {
																																																																																																if ((*(int*)(iVar17 + -0x7dba) < 1000) &&
																																																																																																	(local_4 = 1, 0 < (int)param_1)) {
																																																																																																	local_4 = 1;
																																																																																																	while (true) {
																																																																																																		piVar1 = (int*)(iVar17 + -0x7db0);
																																																																																																		*piVar1 = *piVar1 + 1;
																																																																																																		piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																		*piVar1 = *piVar1 - (*(int*)(iVar17 +
																																																																																																			-0x7db0) >> 1);
																																																																																																		if (local_4 == param_1) break;
																																																																																																		local_4 = local_4 + 1;
																																																																																																	}
																																																																																																}
																																																																																															}
																																																																																															else {
																																																																																																if (uVar9 == 0x61) {
																																																																																																	FUN_20ac_cc18(uVar25);
																																																																																																	local_4 = uVar9;
																																																																																																}
																																																																																																else {
																																																																																																	if (uVar9 == 0x62) {
																																																																																																		if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																			*(undefined*)(iVar17 + -0x7d91) =
																																																																																																				*(undefined*)(iVar17 + -0x7d9c);
																																																																																																			bVar6 = *(byte*)(iVar17 + -0x7d96);
																																																																																																			*(byte*)(iVar17 + -0x7dbd) = bVar6;
																																																																																																			local_4 = (uint)bVar6;
																																																																																																		}
																																																																																																	}
																																																																																																	else {
																																																																																																		if (uVar9 == 100) {
																																																																																																			FUN_20ac_d828(cVar22, uVar25);
																																																																																																			local_4 = uVar9;
																																																																																																		}
																																																																																																		else {
																																																																																																			if (uVar9 == 0x65) {
																																																																																																				FUN_20ac_9a2c(cVar22, uVar25);
																																																																																																				local_4 = uVar9;
																																																																																																			}
																																																																																																			else {
																																																																																																				if (uVar9 == 0x66) {
																																																																																																					if (0 < *(int*)0xd4c4) {
																																																																																																						local_4 = 1;
																																																																																																						if (0 < (int)param_1) {
																																																																																																							local_4 = 1;
																																																																																																							uVar9 = 1;
																																																																																																							uVar8 = param_1;
																																																																																																							while (true) {
																																																																																																								if (*(int*)(iVar17 + -0x7d9e) <
																																																																																																									1) {
																																																																																																									FUN_3ce4_170a(0xe8);
																																																																																																									iVar18 = uVar9 + 200;
																																																																																																									*(int*)(iVar17 + -0x7d9e) =
																																																																																																										iVar18;
																																																																																																									FUN_3ce4_170a(0x90);
																																																																																																									iVar18 = iVar18 + -0x50;
																																																																																																									*(int*)(iVar17 + -0x7da2) =
																																																																																																										iVar18;
																																																																																																									uVar23 = 0x66;
																																																																																																									FUN_3ce4_170a(0x2c);
																																																																																																									uVar7 = (undefined)iVar18;
																																																																																																									FUN_2e48_d001(uVar7);
																																																																																																									FUN_2e48_d15c(2, uVar25, uVar7,
																																																																																																										uVar23);
																																																																																																								}
																																																																																																								else {
																																																																																																									piVar1 = (int*)(iVar17 +
																																																																																																										-0x7d9e);
																																																																																																									*piVar1 = *piVar1 + -1;
																																																																																																								}
																																																																																																								iVar18 = *(int*)(iVar17 + -0x7d9e);
																																																																																																								if ((iVar18 < 2) || (0x50 < iVar18)) {
																																																																																																									if (((iVar18 == 0x51) ||
																																																																																																										((((iVar18 == 0x53 || (iVar18 == 0x55)) ||
																																																																																																											(iVar18 == 0x57)) || (iVar18 == 0x59))))
																																																																																																										&& ((FUN_3ce4_170a(2), iVar18 == 0 &&
																																																																																																											(*(char*)((0x29 - *(int*)0xd4d6 / 100)
																																																																																																												* 0x32 + *(int*)0xd4d2 / 100
																																																																																																												+ 0x423d) == '\0')))) {
																																																																																																										FUN_20ac_bbd2((char)*(undefined2*)0xd4a8,
																																																																																																											(char)*(undefined2*)
																																																																																																											(iVar17 + -0x7da2));
																																																																																																										FUN_2e48_b74c(0x10);
																																																																																																									}
																																																																																																								}
																																																																																																								else {
																																																																																																									FUN_3ce4_170a(3);
																																																																																																									if (iVar18 == 0) {
																																																																																																										piVar1 = (int*)(iVar17 + -0x7d9e);
																																																																																																										*piVar1 = *piVar1 + -1;
																																																																																																									}
																																																																																																								}
																																																																																																								if (local_4 == uVar8) break;
																																																																																																								uVar9 = local_4;
																																																																																																								local_4 = local_4 + 1;
																																																																																																							}
																																																																																																						}
																																																																																																					}
																																																																																																				}
																																																																																																				else {
																																																																																																					if (uVar9 == 0x67) {
																																																																																																						piVar1 = (int*)(iVar17 + -0x7da6);
																																																																																																						*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db2)
																																																																																																							* param_1;
																																																																																																						piVar1 = (int*)(iVar17 + -0x7da2);
																																																																																																						*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db0)
																																																																																																							* param_1;
																																																																																																						piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																						*piVar1 = *piVar1 + (int)*(char*)(iVar17 +
																																																																																																							-0x7da1);
																																																																																																						bVar6 = *(byte*)(iVar17 + -0x7da2);
																																																																																																						local_4 = (uint)bVar6;
																																																																																																						*(uint*)(iVar17 + -0x7da2) = (uint)bVar6;
																																																																																																						FUN_2e48_cf8b(1, 7, (char)*(undefined2*)
																																																																																																							(iVar17 + -0x7db4),
																																																																																																							(char)*(undefined2*)
																																																																																																							(iVar17 + -0x7db8));
																																																																																																						if ((char)local_4 != '\0') {
																																																																																																							FUN_2e48_1af9(1);
																																																																																																							local_4 = local_4 + *(int*)(iVar17 + -0x7db6);
																																																																																																							if (0 < (int)local_4) {
																																																																																																								*(undefined2*)(iVar17 + -0x7dbc) = 0x69;
																																																																																																							}
																																																																																																						}
																																																																																																					}
																																																																																																					else {
																																																																																																						if (uVar9 == 0x68) {
																																																																																																							local_4 = 1;
																																																																																																							if (0 < (int)param_1) {
																																																																																																								local_4 = 1;
																																																																																																								uVar9 = param_1;
																																																																																																								while (true) {
																																																																																																									cVar22 = (char)*(undefined2*)
																																																																																																										(iVar17 + -0x7db8) +
																																																																																																										(char)*(undefined2*)
																																																																																																										(iVar17 + -0x7db2);
																																																																																																									FUN_2e48_cf8b(1, 0x3f, (char)*(undefined2*)
																																																																																																										(iVar17 +
																																																																																																											-0x7db4), cVar22);
																																																																																																									if (cVar22 == '\0') {
																																																																																																										*(int*)(iVar17 + -0x7db2) =
																																																																																																											-*(int*)(iVar17 + -0x7db2);
																																																																																																									}
																																																																																																									else {
																																																																																																										piVar1 = (int*)(iVar17 + -0x7db8);
																																																																																																										*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db2);
																																																																																																									}
																																																																																																									cVar22 = (char)*(undefined2*)(iVar17 + -0x7db4) +
																																																																																																										(char)*(undefined2*)(iVar17 + -0x7dae);
																																																																																																									FUN_2e48_cf8b(1, 0x3f, cVar22,
																																																																																																										(char)*(undefined2*)
																																																																																																										(iVar17 + -0x7db8));
																																																																																																									if (cVar22 == '\0') {
																																																																																																										iVar18 = -*(int*)(iVar17 + -0x7dae);
																																																																																																										*(int*)(iVar17 + -0x7dae) = iVar18;
																																																																																																									}
																																																																																																									else {
																																																																																																										iVar18 = *(int*)(iVar17 + -0x7dae);
																																																																																																										piVar1 = (int*)(iVar17 + -0x7db4);
																																																																																																										*piVar1 = *piVar1 + iVar18;
																																																																																																									}
																																																																																																									if ((*(int*)(iVar17 + -0x7db2) == 0) &&
																																																																																																										(*(int*)(iVar17 + -0x7dae) == 0)) {
																																																																																																										FUN_2e48_0aa0(3);
																																																																																																										*(int*)(iVar17 + -0x7db2) = iVar18;
																																																																																																										FUN_2e48_0aa0(3);
																																																																																																										*(int*)(iVar17 + -0x7dae) = iVar18;
																																																																																																									}
																																																																																																									if (((local_4 == param_1) &&
																																																																																																										(*(char*)0xd4fc != '\0')) &&
																																																																																																										(100 < *(int*)0x5d2)) {
																																																																																																										local_a = -2;
																																																																																																										while (true) {
																																																																																																											local_e = -2;
																																																																																																											while (FUN_20ac_0000(1, (char)local_e * '\x1e'
																																																																																																												+ (char)*(undefined2*)
																																																																																																												(iVar17 +
																																																																																																													-0x7db4), (char)local_a * '\x1e' +
																																																																																																												(char)*(undefined2*)(iVar17 + -0x7db8)),
																																																																																																												local_e != 2) {
																																																																																																												local_e = local_e + 1;
																																																																																																											}
																																																																																																											if (local_a == 2) break;
																																																																																																											local_a = local_a + 1;
																																																																																																										}
																																																																																																									}
																																																																																																									if (local_4 == uVar9) break;
																																																																																																									local_4 = local_4 + 1;
																																																																																																								}
																																																																																																							}
																																																																																																						}
																																																																																																						else {
																																																																																																							if (uVar9 == 0x69) {
																																																																																																								*(undefined*)(iVar17 + -0x7dc1) = 4;
																																																																																																								*(undefined*)(iVar17 + -0x7dc2) = 5;
																																																																																																								piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																								*piVar1 = *piVar1 + param_1 * 2;
																																																																																																								if (100 < *(int*)(iVar17 + -0x7d9a)) {
																																																																																																									*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																								}
																																																																																																								local_16 = *(int*)(iVar17 + -0x7d9a) / 8 +
																																																																																																									0x1b;
																																																																																																								uVar9 = local_16;
																																																																																																								FUN_2e48_00b5(0x24, 0x1e, 0xea, unaff_SS);
																																																																																																								*(undefined*)(iVar17 + -0x7d91) =
																																																																																																									(undefined)local_16;
																																																																																																								local_4 = uVar9 & 0xff00 | local_16 & 0xff;
																																																																																																							}
																																																																																																							else {
																																																																																																								if (uVar9 == 0x6a) {
																																																																																																									local_16 = (*(uint*)0x5d2 >> 4) + 0x1a;
																																																																																																									uVar9 = local_16;
																																																																																																									FUN_2e48_00b5(0x25, 0x1e, 0xea, unaff_SS);
																																																																																																									uVar9 = uVar9 & 0xff00 | local_16 & 0xff;
																																																																																																									*(undefined*)(iVar17 + -0x7d91) =
																																																																																																										(undefined)local_16;
																																																																																																									FUN_3ce4_170a(3);
																																																																																																									local_4 = *(int*)(iVar17 + -0x7da4) + uVar9
																																																																																																										& 0x1ff;
																																																																																																									*(uint*)(iVar17 + -0x7da4) = local_4;
																																																																																																								}
																																																																																																								else {
																																																																																																									if (uVar9 == 0x6c) {
																																																																																																										FUN_3ce4_170a(0x1e);
																																																																																																										if (uVar9 == 0) {
																																																																																																											FUN_2e48_0aa0(8);
																																																																																																											*(uint*)(iVar17 + -0x7db2) = uVar9;
																																																																																																											FUN_2e48_0aa0(8);
																																																																																																											*(uint*)(iVar17 + -0x7dae) = uVar9;
																																																																																																										}
																																																																																																										FUN_3ce4_170a(5);
																																																																																																										local_4 = uVar9;
																																																																																																										if (uVar9 == 0) {
																																																																																																											iVar18 = *(int*)(iVar17 + -0x7db2) -
																																																																																																												*(int*)0x2b0e;
																																																																																																											FUN_2e48_0a25((char)iVar18);
																																																																																																											piVar1 = (int*)0x2b0e;
																																																																																																											*piVar1 = *piVar1 + iVar18;
																																																																																																											local_4 = *(int*)(iVar17 + -0x7dae) -
																																																																																																												*(int*)0x2b10;
																																																																																																											FUN_2e48_0a25((char)local_4);
																																																																																																											piVar1 = (int*)0x2b10;
																																																																																																											*piVar1 = *piVar1 + local_4;
																																																																																																										}
																																																																																																									}
																																																																																																									else {
																																																																																																										if (uVar9 == 0x6d) {
																																																																																																											local_4 = 1;
																																																																																																											if (0 < (int)param_1) {
																																																																																																												local_4 = 1;
																																																																																																												uVar9 = param_1;
																																																																																																												while (FUN_20ac_9b99(uVar25),
																																																																																																													local_4 != uVar9) {
																																																																																																													local_4 = local_4 + 1;
																																																																																																												}
																																																																																																											}
																																																																																																										}
																																																																																																										else {
																																																																																																											if (uVar9 == 0x70) {
																																																																																																												local_4 = 1;
																																																																																																												if (0 < (int)param_1) {
																																																																																																													local_4 = 1;
																																																																																																													uVar9 = 1;
																																																																																																													uVar8 = param_1;
																																																																																																													while (true) {
																																																																																																														*(undefined*)(iVar17 + -0x7dc0) =
																																																																																																															2;
																																																																																																														if (3 < *(byte*)(iVar17 + -0x7dc1
																																																																																																															)) {
																																																																																																															uVar9 = 0;
																																																																																																															*(undefined2*)
																																																																																																																(iVar17 + -0x7dba) = 0;
																																																																																																														}
																																																																																																														FUN_2e48_1b45((char)*(undefined2*
																																																																																																															)(iVar17 +
																																																																																																																-0x7db4), (char)*(undefined2*)(iVar17 + -0x7db6),
																																																																																																															(char)*(undefined2*)(iVar17 + -0x7db8));
																																																																																																														*(uint*)(iVar17 + -0x7db6) = uVar9;
																																																																																																														FUN_3ce4_170a(0x32);
																																																																																																														if (uVar9 == 0) {
																																																																																																															FUN_2e48_0a25((char)*(undefined2*)0x2b0e);
																																																																																																															*(uint*)(iVar17 + -0x7db2) = uVar9;
																																																																																																															FUN_2e48_0a25((char)*(undefined2*)0x2b10);
																																																																																																															*(uint*)(iVar17 + -0x7dae) = uVar9;
																																																																																																														}
																																																																																																														iVar18 = *(int*)(iVar17 + -0x7db4) +
																																																																																																															*(int*)(iVar17 + -0x7dae);
																																																																																																														FUN_2e48_cf8b(1, 7, (char)iVar18,
																																																																																																															(char)*(undefined2*)
																																																																																																															(iVar17 + -0x7db8) +
																																																																																																															(char)*(undefined2*)
																																																																																																															(iVar17 + -0x7db2));
																																																																																																														cVar22 = (char)iVar18;
																																																																																																														FUN_3ce4_170a(4);
																																																																																																														if ((iVar18 == 0) && (cVar22 != '\0')) {
																																																																																																															iVar18 = *(int*)(iVar17 + -0x7db2);
																																																																																																															piVar1 = (int*)(iVar17 + -0x7db8);
																																																																																																															*piVar1 = *piVar1 + iVar18;
																																																																																																														}
																																																																																																														FUN_3ce4_170a(4);
																																																																																																														if ((iVar18 == 0) && (cVar22 != '\0')) {
																																																																																																															piVar1 = (int*)(iVar17 + -0x7db4);
																																																																																																															*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7dae);
																																																																																																														}
																																																																																																														if (cVar22 == '\0') {
																																																																																																															if (*(int*)(iVar17 + -0x7da2) < 1) {
																																																																																																																*(undefined2*)(iVar17 + -0x7dbc) = 0x6d;
																																																																																																															}
																																																																																																															else {
																																																																																																																piVar1 = (int*)(iVar17 + -0x7da2);
																																																																																																																*piVar1 = *piVar1 + -1;
																																																																																																															}
																																																																																																														}
																																																																																																														if (local_4 == uVar8) break;
																																																																																																														uVar9 = local_4;
																																																																																																														local_4 = local_4 + 1;
																																																																																																													}
																																																																																																												}
																																																																																																											}
																																																																																																											else {
																																																																																																												if (uVar9 == 0x6e) {
																																																																																																													*(int*)(iVar17 + -0x7d9a) =
																																																																																																														*(int*)0x5d2 + -0x32 >> 6;
																																																																																																													uVar9 = *(int*)(iVar17 + -0x7d9e) +
																																																																																																														*(int*)(iVar17 + -0x7dae) & 0x1ff;
																																																																																																													*(uint*)(iVar17 + -0x7d9e) = uVar9;
																																																																																																													FUN_3ce4_170a(0x28);
																																																																																																													if (uVar9 == 0) {
																																																																																																														FUN_2e48_0aa0(4);
																																																																																																														*(uint*)(iVar17 + -0x7dae) = uVar9;
																																																																																																													}
																																																																																																													FUN_2e48_0051((char)*(undefined2*)
																																																																																																														(iVar17 + -0x7d9e));
																																																																																																													bStack69 = (byte)((uint)uVar19 >> 8);
																																																																																																													sVar3 = (sqword)CONCAT13(bStack69, CONCAT12((
																																																																																																														char)uVar19, uVar9)) * 5;
																																																																																																													iVar18 = (int)((qword)sVar3 >> 0x10);
																																																																																																													uVar7 = (undefined)((qword)sVar3 >> 0x10);
																																																																																																													uVar23 = (undefined)((qword)sVar3 >> 0x18);
																																																																																																													FUN_2e48_1b45((char)*(undefined2*)
																																																																																																														(iVar17 + -0x7db4),
																																																																																																														(char)*(undefined2*)
																																																																																																														(iVar17 + -0x7db6),
																																																																																																														(char)*(undefined2*)
																																																																																																														(iVar17 + -0x7db8));
																																																																																																													local_4 = iVar18 + 6 + CONCAT11(uVar23, uVar7);
																																																																																																													*(uint*)(iVar17 + -0x7db6) = local_4;
																																																																																																												}
																																																																																																												else {
																																																																																																													if (uVar9 == 0x6f) {
																																																																																																														*(undefined2*)0x2b10 = 0xffec;
																																																																																																														*(undefined2*)0x2b0e = 0;
																																																																																																														piVar1 = (int*)(iVar17 + -0x7d98);
																																																																																																														*piVar1 = *piVar1 - param_1;
																																																																																																														local_4 = param_1;
																																																																																																														if (*(int*)(iVar17 + -0x7d98) < 1) {
																																																																																																															*(uint*)0x5cc = local_6;
																																																																																																															if (*(char*)(iVar17 + -0x7d90) != '\0') {
																																																																																																																*(undefined2*)0xd4c4 = 0;
																																																																																																															}
																																																																																																															*(undefined2*)0xdc4c =
																																																																																																																*(undefined2*)(iVar17 + -0x7da6);
																																																																																																															*(undefined2*)0xd4d0 =
																																																																																																																*(undefined2*)0xdc4c;
																																																																																																															if (0 < *(int*)(iVar17 + -0x7da2)) {
																																																																																																																FUN_20ac_3d30(uVar25);
																																																																																																															}
																																																																																																															*(undefined2*)(iVar17 + -0x7dba) = 1000;
																																																																																																															*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																															local_4 = 0;
																																																																																																														}
																																																																																																													}
																																																																																																													else {
																																																																																																														if (uVar9 == 0x71) {
																																																																																																															local_4 = *(int*)(iVar17 + -0x7da2) *
																																																																																																																param_1 + *(int*)(iVar17 +
																																																																																																																	-0x7da0);
																																																																																																															uVar11 = (ulong)local_4;
																																																																																																															while (true) {
																																																																																																																uVar9 = (uint)uVar11;
																																																																																																																if (local_4._1_1_ == '\0') break;
																																																																																																																FUN_2e48_2049();
																																																																																																																local_4 = local_4 - 0x100;
																																																																																																																uVar26 = 0xee;
																																																																																																																uVar21 = 0xec;
																																																																																																																uVar8 = uVar9;
																																																																																																																uVar7 = unaff_SS;
																																																																																																																uVar23 = unaff_SS;
																																																																																																																FUN_2e48_0aa0(100);
																																																																																																																iVar18 = (0x100 - *(int*)0xdc4c) + uVar8;
																																																																																																																uVar20 = (undefined)iVar18;
																																																																																																																FUN_3ce4_170a(0xfa);
																																																																																																																iVar18 = iVar18 + 100;
																																																																																																																FUN_2e48_2e58(iVar18, uVar20, uVar21, uVar7, uVar26,
																																																																																																																	uVar23);
																																																																																																																uVar7 = (undefined)iVar18;
																																																																																																																FUN_2e48_2049();
																																																																																																																cVar22 = (char)*(undefined2*)0xd4d2;
																																																																																																																cVar28 = cVar22;
																																																																																																																FUN_3ce4_170a(0x46);
																																																																																																																uVar8 = *(uint*)0xd4d6;
																																																																																																																uVar11 = (ulong)uVar8;
																																																																																																																FUN_20ac_d682((char)uVar8,
																																																																																																																	(char)*(undefined2*)0xd4d4 + 'd'
																																																																																																																	+ cVar28, cVar22, uVar7);
																																																																																																																local_16 = uVar9;
																																																																																																															}
																																																																																																															*(uint*)(iVar17 + -0x7da0) = local_4;
																																																																																																														}
																																																																																																														else {
																																																																																																															if (uVar9 == 0x72) {
																																																																																																																piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																																*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db0)
																																																																																																																	* param_1;
																																																																																																																piVar1 = (int*)(iVar17 + -0x7db8);
																																																																																																																*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db2)
																																																																																																																	;
																																																																																																																local_16 = *(uint*)(iVar17 + -0x7dae);
																																																																																																																piVar1 = (int*)(iVar17 + -0x7db4);
																																																																																																																*piVar1 = *piVar1 + local_16;
																																																																																																																FUN_2e48_1b45((char)*(undefined2*)
																																																																																																																	(iVar17 + -0x7db4),
																																																																																																																	(char)*(undefined2*)
																																																																																																																	(iVar17 + -0x7db6),
																																																																																																																	(char)*(undefined2*)
																																																																																																																	(iVar17 + -0x7db8));
																																																																																																																local_4 = *(uint*)(iVar17 + -0x7db6);
																																																																																																																if ((int)local_4 < (int)local_16) {
																																																																																																																	FUN_3ce4_170a(4);
																																																																																																																	*(int*)(iVar17 + -0x7dba) = local_4 + 1;
																																																																																																																	*(undefined*)(iVar17 + -0x7dc1) = 2;
																																																																																																																	*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																	*(uint*)(iVar17 + -0x7db6) = local_16;
																																																																																																																	local_4 = local_16;
																																																																																																																}
																																																																																																															}
																																																																																																															else {
																																																																																																																if (uVar9 == 0x74) {
																																																																																																																	local_4 = 1;
																																																																																																																	if (0 < (int)param_1) {
																																																																																																																		local_4 = 1;
																																																																																																																		uVar9 = 1;
																																																																																																																		uVar8 = param_1;
																																																																																																																		while (true) {
																																																																																																																			if (*(int*)(iVar17 + -0x7da2) < 1) {
																																																																																																																				*(undefined*)(iVar17 + -0x7d91) =
																																																																																																																					0x23;
																																																																																																																				FUN_3ce4_170a(0x32);
																																																																																																																				if (uVar9 == 0) {
																																																																																																																					FUN_3ce4_170a(100);
																																																																																																																					*(int*)(iVar17 + -0x7da0) =
																																																																																																																						uVar9 + 10;
																																																																																																																				}
																																																																																																																				iVar18 = *(int*)(iVar17 + -0x7da0) +
																																																																																																																					1;
																																																																																																																				FUN_3ce4_170a((char)iVar18);
																																																																																																																				if (iVar18 == 0) {
																																																																																																																					FUN_3ce4_170a(3);
																																																																																																																					*(int*)(iVar17 + -0x7da2) =
																																																																																																																						iVar18 + 2;
																																																																																																																				}
																																																																																																																			}
																																																																																																																			else {
																																																																																																																				*(undefined*)(iVar17 + -0x7d91) = 5;
																																																																																																																				if ((1 < *(int*)(iVar17 + -0x7da2))
																																																																																																																					&& (FUN_3ce4_170a((char)*(
																																																																																																																						undefined2*)(iVar17 + -0x7da2)), uVar9 == 0)) {
																																																																																																																					*(undefined2*)(iVar17 + -0x7da2) = 0;
																																																																																																																				}
																																																																																																																				if (local_4 == param_1) {
																																																																																																																					local_12 = 0;
																																																																																																																					while (true) {
																																																																																																																						local_14 = 0;
																																																																																																																						while (FUN_20ac_0000(0x32, (char)local_14 * '2'
																																																																																																																							+ (char)*(undefined2
																																																																																																																								*)(iVar17
																																																																																																																									+ -0x7db4) + -0x19,
																																																																																																																							(char)local_12 * '2' +
																																																																																																																							(char)*(undefined2*)(iVar17 + -0x7db8) + -0x19),
																																																																																																																							local_14 != 1) {
																																																																																																																							local_14 = local_14 + 1;
																																																																																																																						}
																																																																																																																						if (local_12 == 1) break;
																																																																																																																						local_12 = local_12 + 1;
																																																																																																																					}
																																																																																																																				}
																																																																																																																			}
																																																																																																																			if (local_4 == uVar8) break;
																																																																																																																			uVar9 = local_4;
																																																																																																																			local_4 = local_4 + 1;
																																																																																																																		}
																																																																																																																	}
																																																																																																																}
																																																																																																																else {
																																																																																																																	if (uVar9 == 0x75) {
																																																																																																																		if (*(char*)0xd500 != '\0') {
																																																																																																																			FUN_20ac_af92(cVar22, uVar25);
																																																																																																																			local_4 = uVar9;
																																																																																																																		}
																																																																																																																	}
																																																																																																																	else {
																																																																																																																		if (uVar9 == 0x76) {
																																																																																																																			uVar7 = (undefined)
																																																																																																																				* (undefined2*)(iVar17 + -0x7db6);
																																																																																																																			FUN_2e48_18cf((char)*(undefined2*)
																																																																																																																				(iVar17 + -0x7db4),
																																																																																																																				(char)*(undefined2*)
																																																																																																																				(iVar17 + -0x7db6),
																																																																																																																				(char)*(undefined2*)
																																																																																																																				(iVar17 + -0x7db8));
																																																																																																																			FUN_2e48_0a6c(cVar22, (char)uVar9, uVar7);
																																																																																																																			piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																																			*piVar1 = *piVar1 + uVar9;
																																																																																																																			local_4 = uVar9;
																																																																																																																		}
																																																																																																																		else {
																																																																																																																			if (uVar9 == 0x77) {
																																																																																																																				if (*(int*)(*(int*)(iVar17 + -0x7d9c) *
																																																																																																																					0x36 + -0x7dbc) == 0) {
																																																																																																																					*(undefined2*)(iVar17 + -0x7dbc) = 0x78
																																																																																																																						;
																																																																																																																					*(undefined*)(iVar17 + -0x7dbd) = 3;
																																																																																																																					*(undefined2*)(iVar17 + -0x7d9c) = 100;
																																																																																																																				}
																																																																																																																			}
																																																																																																																			else {
																																																																																																																				if (uVar9 == 0x78) {
																																																																																																																					if ((*(int*)(iVar17 + -0x7d9c) < 0x14)
																																																																																																																						&& (FUN_2e48_2d63(uVar25),
																																																																																																																							(char)uVar9 == '\0')) {
																																																																																																																						piVar1 = (int*)(iVar17 + -0x7d9c);
																																																																																																																						*piVar1 = *piVar1 + 1;
																																																																																																																					}
																																																																																																																					*(undefined*)(iVar17 + -0x7dbe) = 0x78;
																																																																																																																					local_4 = uVar9;
																																																																																																																				}
																																																																																																																				else {
																																																																																																																					if (uVar9 == 0x79) {
																																																																																																																						FUN_2e48_2d63(uVar25);
																																																																																																																						local_4 = uVar9;
																																																																																																																						if (((char)uVar9 != '\0') &&
																																																																																																																							(*(int*)(iVar17 + -0x7dba) == 1000
																																																																																																																								)) {
																																																																																																																							FUN_3ce4_1b69(6, 0x7e, uVar24,
																																																																																																																								cVar27 + 'H', uVar24);
																																																																																																																							*(undefined2*)(iVar17 + -0x7dba) =
																																																																																																																								0;
																																																																																																																							local_4 = 0;
																																																																																																																						}
																																																																																																																					}
																																																																																																																					else {
																																																																																																																						if (uVar9 == 0x7a) {
																																																																																																																							if ((*(int*)(iVar17 + -0x7dba) ==
																																																																																																																								1000) &&
																																																																																																																								(local_4 = 1, 0 < (int)param_1))
																																																																																																																							{
																																																																																																																								local_4 = 1;
																																																																																																																								uVar9 = param_1;
																																																																																																																								while (FUN_20ac_ac3e(uVar25),
																																																																																																																									local_4 != uVar9) {
																																																																																																																									local_4 = local_4 + 1;
																																																																																																																								}
																																																																																																																							}
																																																																																																																						}
																																																																																																																						else {
																																																																																																																							if (uVar9 == 0x7b) {
																																																																																																																								if ((*(int*)(iVar17 + -0x7dba) ==
																																																																																																																									1000) &&
																																																																																																																									(local_4 = 1, 0 < (int)param_1)
																																																																																																																									) {
																																																																																																																									local_4 = 1;
																																																																																																																									uVar9 = param_1;
																																																																																																																									while (FUN_20ac_ad6e(uVar25),
																																																																																																																										local_4 != uVar9) {
																																																																																																																										local_4 = local_4 + 1;
																																																																																																																									}
																																																																																																																								}
																																																																																																																							}
																																																																																																																							else {
																																																																																																																								if (uVar9 == 0x7c) {
																																																																																																																									if ((*(char*)0xd500 != '\0') &&
																																																																																																																										(FUN_2e48_2b33(uVar25),
																																																																																																																											local_4 = uVar9, uVar9 == 0))
																																																																																																																									{
																																																																																																																										local_4 = *(uint*)(iVar17 +
																																																																																																																											-0x7da2);
																																																																																																																										*(uint*)(iVar17 + -0x7dbc) = local_4;
																																																																																																																									}
																																																																																																																								}
																																																																																																																								else {
																																																																																																																									if (uVar9 == 0x7d) {
																																																																																																																										local_4 = 1;
																																																																																																																										if (0 < (int)param_1) {
																																																																																																																											local_4 = 1;
																																																																																																																											uVar9 = param_1;
																																																																																																																											while (true) {
																																																																																																																												if ((*(byte*)(iVar17 + -0x7d91) < 0x29)
																																																																																																																													|| (0x72 < *(byte*)(iVar17 + -0x7d91))
																																																																																																																													) {
																																																																																																																													if (*(char*)(iVar17 + -0x7d91) ==
																																																																																																																														'\x16') {
																																																																																																																														*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																													}
																																																																																																																												}
																																																																																																																												else {
																																																																																																																													FUN_20ac_9de1(uVar25);
																																																																																																																												}
																																																																																																																												if (local_4 == uVar9) break;
																																																																																																																												local_4 = local_4 + 1;
																																																																																																																											}
																																																																																																																										}
																																																																																																																									}
																																																																																																																									else {
																																																																																																																										if (uVar9 == 0x7e) {
																																																																																																																											if ((*(char*)0xd500 != '\0') &&
																																																																																																																												((*(int*)(iVar17 + -0x7d98) < 0 ||
																																																																																																																													(FUN_2e48_2d63(uVar25), local_4 = uVar9,
																																																																																																																														(char)uVar9 != '\0')))) {
																																																																																																																												local_4 = *(uint*)(iVar17 + -0x7da2);
																																																																																																																												*(uint*)(iVar17 + -0x7dbc) = local_4;
																																																																																																																											}
																																																																																																																										}
																																																																																																																										else {
																																																																																																																											if (uVar9 == 0x7f) {
																																																																																																																												*(undefined*)(iVar17 + -0x7dbd) = 2;
																																																																																																																												FUN_2e48_2b33(uVar25);
																																																																																																																												*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																																																												local_4 = uVar9;
																																																																																																																												if (((1 < (int)uVar9) &&
																																																																																																																													(*(int*)(uVar9 * 0x36 + -0x7dbc) == 2
																																																																																																																														)) && (*(char*)(uVar9 * 0x36 +
																																																																																																																															-0x7d90) == '\x05')) {
																																																																																																																													FUN_1000_0fda(uVar25);
																																																																																																																													local_4 = uVar9;
																																																																																																																												}
																																																																																																																											}
																																																																																																																											else {
																																																																																																																												if (uVar9 == 0x80) {
																																																																																																																													FUN_2e48_2d63(uVar25);
																																																																																																																													local_4 = uVar9;
																																																																																																																													if ((char)uVar9 != '\0') {
																																																																																																																														local_16 = *(int*)(iVar17 + -0x7db4)
																																																																																																																															/ 100;
																																																																																																																														iVar18 = (int)(*(int*)(iVar17 +
																																																																																																																															-0x7dae) * param_1) / 100;
																																																																																																																														piVar1 = (int*)(iVar17 + -0x7dae);
																																																																																																																														*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7db2);
																																																																																																																														piVar1 = (int*)(iVar17 + -0x7da4);
																																																																																																																														*piVar1 = *piVar1 + (iVar18 >> 1);
																																																																																																																														uVar7 = *(undefined*)(iVar17 + -0x7dbf);
																																																																																																																														uVar23 = uVar7;
																																																																																																																														FUN_2e48_d001(0);
																																																																																																																														FUN_2e48_d15c(2, uVar25, uVar23, uVar7);
																																																																																																																														*(int*)(iVar17 + -0x7db4) =
																																																																																																																															local_16 * 100 +
																																																																																																																															(*(int*)(iVar17 + -0x7db4) + iVar18) % 100;
																																																																																																																														*(undefined2*)0xdc4c =
																																																																																																																															*(undefined2*)(iVar17 + -0x7da6);
																																																																																																																														local_4 = *(uint*)0xdc4c;
																																																																																																																														*(uint*)0xd4d0 = local_4;
																																																																																																																														if (*(int*)0x8294 == 0) {
																																																																																																																															*(undefined2*)0x8294 = 300;
																																																																																																																														}
																																																																																																																													}
																																																																																																																												}
																																																																																																																												else {
																																																																																																																													if (uVar9 == 0x81) {
																																																																																																																														local_4 = 1;
																																																																																																																														if (0 < (int)param_1) {
																																																																																																																															local_4 = 1;
																																																																																																																															uVar9 = param_1;
																																																																																																																															while (FUN_20ac_c52e(0x23, uVar25),
																																																																																																																																local_4 != uVar9) {
																																																																																																																																local_4 = local_4 + 1;
																																																																																																																															}
																																																																																																																														}
																																																																																																																													}
																																																																																																																													else {
																																																																																																																														if (uVar9 == 0x82) {
																																																																																																																															if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																local_4 = 1;
																																																																																																																																if (0 < (int)param_1) {
																																																																																																																																	local_4 = 1;
																																																																																																																																	uVar9 = 1;
																																																																																																																																	uVar8 = param_1;
																																																																																																																																	while (true) {
																																																																																																																																		FUN_3ce4_170a(2);
																																																																																																																																		if (uVar9 == 0) {
																																																																																																																																			FUN_2e48_d15c(1, uVar25, (char)*(
																																																																																																																																				undefined2*)0x5d0, 10);
																																																																																																																																		}
																																																																																																																																		else {
																																																																																																																																			FUN_2e48_d15c(1, uVar25, (char)*(undefined2*)
																																																																																																																																				0x5d0, 0x2b);
																																																																																																																																		}
																																																																																																																																		uVar9 = *(uint*)(iVar17 + -0x7dae);
																																																																																																																																		uVar10 = (int)uVar9 >> 0xf;
																																																																																																																																		uVar16 = *(uint*)(iVar17 + -0x7db2);
																																																																																																																																		uVar14 = (int)uVar16 >> 0xf;
																																																																																																																																		piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																																																		*piVar1 = *piVar1 + ((uVar16 ^ uVar14) - uVar14) +
																																																																																																																																			((uVar9 ^ uVar10) - uVar10);
																																																																																																																																		while (100 < *(int*)(iVar17 + -0x7d9a)) {
																																																																																																																																			piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																																																			*piVar1 = *piVar1 + -100;
																																																																																																																																			piVar1 = (int*)(iVar17 + -0x7da2);
																																																																																																																																			*piVar1 = *piVar1 + -1;
																																																																																																																																			if (*(int*)(iVar17 + -0x7da2) < 1) {
																																																																																																																																				*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																																			}
																																																																																																																																			iVar12 = (int)(char)(*(int*)(iVar17 + -0x7db8)
																																																																																																																																				/ 100);
																																																																																																																																			iVar18 = 0x29 - (char)(*(int*)(iVar17 + -0x7db4
																																																																																																																																				) / 100);
																																																																																																																																			if (*(char*)0xd500 != '\0') {
																																																																																																																																				*(char*)(iVar18 * 0x32 + iVar12 + 0x3a6d) =
																																																																																																																																					(*(byte*)(iVar18 * 0x32 + iVar12 +
																																																																																																																																						0x3a6d) & 0xe0) +
																																																																																																																																					(*(byte*)(iVar17 + -0x7d90) & 0x1f);
																																																																																																																																				iVar18 = iVar12;
																																																																																																																																				FUN_20ac_01d6(2, 0x80, 0x80, 1, 1, 0xfd, 0xfd,
																																																																																																																																					(char)*(undefined2*)
																																																																																																																																					(iVar17 + -0x7db4),
																																																																																																																																					(char)*(undefined2*)
																																																																																																																																					(iVar17 + -0x7db8));
																																																																																																																																			}
																																																																																																																																			FUN_2e48_0a25((char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7db2));
																																																																																																																																			iVar18 = iVar18 * 100;
																																																																																																																																			piVar1 = (int*)(iVar17 + -0x7db8);
																																																																																																																																			*piVar1 = *piVar1 + iVar18;
																																																																																																																																			FUN_2e48_0a25((char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7dae));
																																																																																																																																			piVar1 = (int*)(iVar17 + -0x7db4);
																																																																																																																																			*piVar1 = *piVar1 + iVar18 * 100;
																																																																																																																																		}
																																																																																																																																		if (local_4 == uVar8) break;
																																																																																																																																		uVar9 = local_4;
																																																																																																																																		local_4 = local_4 + 1;
																																																																																																																																	}
																																																																																																																																}
																																																																																																																															}
																																																																																																																															else {
																																																																																																																																FUN_2e48_18cf((char)*(undefined2*)
																																																																																																																																	(iVar17 + -0x7db4),
																																																																																																																																	(char)*(undefined2*)
																																																																																																																																	(iVar17 + -0x7db6),
																																																																																																																																	(char)*(undefined2*)
																																																																																																																																	(iVar17 + -0x7db8));
																																																																																																																																*(uint*)(iVar17 + -0x7db6) = uVar9 - 4;
																																																																																																																																local_4 = uVar9 - 4;
																																																																																																																															}
																																																																																																																														}
																																																																																																																														else {
																																																																																																																															if (uVar9 == 0x83) {
																																																																																																																																local_4 = 1;
																																																																																																																																if (0 < (int)param_1) {
																																																																																																																																	local_4 = 1;
																																																																																																																																	uVar9 = param_1;
																																																																																																																																	while (true) {
																																																																																																																																		if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																			FUN_20ac_b33c(uVar25);
																																																																																																																																		}
																																																																																																																																		if (local_4 == uVar9) break;
																																																																																																																																		local_4 = local_4 + 1;
																																																																																																																																	}
																																																																																																																																}
																																																																																																																															}
																																																																																																																															else {
																																																																																																																																if (uVar9 == 0x85) {
																																																																																																																																	if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																		FUN_2e48_0a6c(cVar22, 100,
																																																																																																																																			(char)*(undefined2*)
																																																																																																																																			(iVar17 + -0x7d9a));
																																																																																																																																		piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																																																		*piVar1 = *piVar1 + uVar9;
																																																																																																																																		local_4 = uVar9;
																																																																																																																																	}
																																																																																																																																}
																																																																																																																																else {
																																																																																																																																	if (uVar9 == 0x86) {
																																																																																																																																		local_4 = *(int*)(iVar17 + -0x7d9c) *
																																																																																																																																			param_1;
																																																																																																																																		FUN_2e48_0a6c((char)local_4,
																																																																																																																																			(char)*(undefined2*)
																																																																																																																																			(iVar17 + -0x7da0),
																																																																																																																																			(char)*(undefined2*)
																																																																																																																																			(iVar17 + -0x7d9a));
																																																																																																																																		piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																																																		*piVar1 = *piVar1 + local_4;
																																																																																																																																	}
																																																																																																																																	else {
																																																																																																																																		if (uVar9 == 0x87) {
																																																																																																																																			*(undefined2*)(iVar17 + -0x7daa) = 200;
																																																																																																																																			piVar1 = (int*)(iVar17 + -0x7db8);
																																																																																																																																			*piVar1 = *piVar1 + *(int*)(iVar17 +
																																																																																																																																				-0x7db2) * param_1;
																																																																																																																																			piVar1 = (int*)(iVar17 + -0x7db4);
																																																																																																																																			*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7dae) *
																																																																																																																																				param_1;
																																																																																																																																			iVar18 = *(int*)(iVar17 + -0x7db0) * param_1;
																																																																																																																																			piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																																																			*piVar1 = *piVar1 + iVar18;
																																																																																																																																			FUN_2e48_2049();
																																																																																																																																			cVar22 = (char)*(undefined2*)(iVar17 + -0x7db4) +
																																																																																																																																				(char)*(undefined2*)(iVar17 + -0x7dae) *
																																																																																																																																				-10;
																																																																																																																																			FUN_20ac_044e(0x82, 2, 0x14, cVar22,
																																																																																																																																				(char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7db6) +
																																																																																																																																				(char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7db0) * -10,
																																																																																																																																				(char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7db8) +
																																																																																																																																				(char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7db2) * -10,
																																																																																																																																				(char)iVar18);
																																																																																																																																			FUN_3ce4_170a(0x50);
																																																																																																																																			*(char*)(iVar18 * 0x36 + -0x7d8f) = cVar22 + 'P';
																																																																																																																																			*(undefined2*)(iVar18 * 0x36 + -0x7db0) = 0;
																																																																																																																																			*(undefined2*)0xd4b6 = 300;
																																																																																																																																			local_16 = *(int*)0xd4d6 -
																																																																																																																																				*(int*)(iVar17 + -0x7db4);
																																																																																																																																			FUN_2e48_2ec4(0x4c, uVar24, (char)local_16,
																																																																																																																																				(char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7db8) -
																																																																																																																																				(char)*(undefined2*)0xd4d2);
																																																																																																																																			*(undefined*)(iVar17 + -0x7dbf) = 0x3c;
																																																																																																																																			FUN_2e48_00e4(0x88, 0, (char)*(undefined2*)
																																																																																																																																				(iVar17 + -0x7db8));
																																																																																																																																			if (((char)local_16 == '\0') &&
																																																																																																																																				(FUN_2e48_00e4(0xa0, 0, (char)*(undefined2*)
																																																																																																																																					(iVar17 + -0x7db4)
																																																																																																																																				), (char)local_16 == '\0')) {
																																																																																																																																				*(undefined*)(iVar17 + -0x7dbf) = 0x41;
																																																																																																																																				FUN_2e48_18cf((char)*(undefined2*)
																																																																																																																																					(iVar17 + -0x7db4),
																																																																																																																																					(char)*(undefined2*)
																																																																																																																																					(iVar17 + -0x7db6),
																																																																																																																																					(char)*(undefined2*)
																																																																																																																																					(iVar17 + -0x7db8));
																																																																																																																																			}
																																																																																																																																			else {
																																																																																																																																				local_16 = 0xfe0c;
																																																																																																																																			}
																																																																																																																																			uVar7 = *(undefined*)(iVar17 + -0x7dbf);
																																																																																																																																			uVar23 = uVar7;
																																																																																																																																			FUN_2e48_d001(0);
																																																																																																																																			FUN_2e48_d15c(1, uVar25, uVar23, uVar7);
																																																																																																																																			if (*(int*)(iVar17 + -0x7db6) < (int)local_16) {
																																																																																																																																				*(undefined*)(iVar17 + -0x7d91) = 0xff;
																																																																																																																																				*(undefined2*)(iVar17 + -0x7dbc) = 0x88;
																																																																																																																																				FUN_2e48_b74c(0x10);
																																																																																																																																				*(undefined2*)(iVar17 + -0x7db2) = 0;
																																																																																																																																				*(undefined2*)(iVar17 + -0x7dae) = 0;
																																																																																																																																				uVar7 = 0;
																																																																																																																																				*(undefined2*)(iVar17 + -0x7db0) = 0;
																																																																																																																																				uVar23 = 0x20;
																																																																																																																																				FUN_2e48_d001(0);
																																																																																																																																				FUN_2e48_d15c(1, 0xff, uVar7, uVar23);
																																																																																																																																				uVar23 = 0x19;
																																																																																																																																				FUN_2e48_d001(0);
																																																																																																																																				FUN_2e48_d15c(1, 0xfe, uVar7, uVar23);
																																																																																																																																			}
																																																																																																																																			local_a = -2;
																																																																																																																																			while (true) {
																																																																																																																																				local_e = -2;
																																																																																																																																				while (true) {
																																																																																																																																					cVar22 = (char)local_a * '\x1e' +
																																																																																																																																						(char)*(undefined2*)
																																																																																																																																						(iVar17 + -0x7db8);
																																																																																																																																					local_4 = local_e * 0x1e +
																																																																																																																																						*(int*)(iVar17 + -0x7db4);
																																																																																																																																					uVar7 = (undefined)local_4;
																																																																																																																																					FUN_3ce4_170a(0x1e);
																																																																																																																																					FUN_20ac_0000((char)local_4, uVar7, cVar22);
																																																																																																																																					if (local_e == 2) break;
																																																																																																																																					local_e = local_e + 1;
																																																																																																																																				}
																																																																																																																																				if (local_a == 2) break;
																																																																																																																																				local_a = local_a + 1;
																																																																																																																																			}
																																																																																																																																		}
																																																																																																																																		else {
																																																																																																																																			if (uVar9 == 0x88) {
																																																																																																																																				FUN_20ac_b5f4(cVar22, uVar25);
																																																																																																																																				local_4 = uVar9;
																																																																																																																																			}
																																																																																																																																			else {
																																																																																																																																				if (uVar9 == 0x89) {
																																																																																																																																					FUN_3ce4_170a(0x32);
																																																																																																																																					*(uint*)(iVar17 + -0x7dba) = uVar9 + 0x46;
																																																																																																																																					local_4 = uVar9 + 0x46;
																																																																																																																																				}
																																																																																																																																				else {
																																																																																																																																					if (uVar9 == 0x8a) {
																																																																																																																																						piVar1 = (int*)(iVar17 + -0x7db8);
																																																																																																																																						*piVar1 = *piVar1 + *(int*)(iVar17 +
																																																																																																																																							-0x7db2) * param_1;
																																																																																																																																						piVar1 = (int*)(iVar17 + -0x7db4);
																																																																																																																																						*piVar1 = *piVar1 + *(int*)(iVar17 + -0x7dae) *
																																																																																																																																							param_1;
																																																																																																																																						iVar18 = *(int*)(iVar17 + -0x7db0) * param_1;
																																																																																																																																						piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																																																						*piVar1 = *piVar1 + iVar18;
																																																																																																																																						uVar7 = 0x16;
																																																																																																																																						FUN_2e48_d001(0);
																																																																																																																																						FUN_2e48_d15c(1, uVar25, (char)iVar18, uVar7);
																																																																																																																																						FUN_3ce4_170a(5);
																																																																																																																																						if (iVar18 == 0) {
																																																																																																																																							FUN_2e48_2049();
																																																																																																																																							FUN_20ac_044e(0x82, 2, 0x14,
																																																																																																																																								(char)*(undefined2*)
																																																																																																																																								(iVar17 + -0x7db4) + -10,
																																																																																																																																								(char)*(undefined2*)
																																																																																																																																								(iVar17 + -0x7db6),
																																																																																																																																								(char)*(undefined2*)
																																																																																																																																								(iVar17 + -0x7db8),
																																																																																																																																								(char)iVar18);
																																																																																																																																						}
																																																																																																																																						local_4 = *(int*)(iVar17 + -0x7db4) +
																																																																																																																																							*(int*)(iVar17 + -0x7dae);
																																																																																																																																						FUN_2e48_cf8b(8, 8, (char)local_4,
																																																																																																																																							(char)*(undefined2*)
																																																																																																																																							(iVar17 + -0x7db8) +
																																																																																																																																							(char)*(undefined2*)
																																																																																																																																							(iVar17 + -0x7db2));
																																																																																																																																						if ((char)local_4 == '\0') {
																																																																																																																																							FUN_20ac_b2c8(0x1d, 0, (char)*(undefined2*)
																																																																																																																																								(iVar17 + -0x7db4),
																																																																																																																																								(char)*(undefined2*)
																																																																																																																																								(iVar17 + -0x7db8));
																																																																																																																																						}
																																																																																																																																						else {
																																																																																																																																							*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																																							local_4 = 0;
																																																																																																																																						}
																																																																																																																																					}
																																																																																																																																					else {
																																																																																																																																						if (uVar9 == 0x8b) {
																																																																																																																																							FUN_3ce4_170a(0xe8);
																																																																																																																																							local_4 = *(uint*)(iVar17 + -0x7d96);
																																																																																																																																							if (((int)local_4 < 0 &&
																																																																																																																																								-1 < (int)local_4 >> 0xf) ||
																																																																																																																																								((-1 < (int)local_4 >> 0xf &&
																																																																																																																																									(uVar9 < local_4)))) {
																																																																																																																																								FUN_2e48_2049();
																																																																																																																																								uVar9 = *(int*)(iVar17 + -0x7db6) +
																																																																																																																																									*(int*)(iVar17 + -0x7daa);
																																																																																																																																								FUN_20ac_044e(1, 1, 5, (char)*(undefined2*)
																																																																																																																																									(iVar17 + -0x7db4
																																																																																																																																										), (char)uVar9,
																																																																																																																																									(char)*(undefined2*)
																																																																																																																																									(iVar17 + -0x7db8),
																																																																																																																																									(char)local_4);
																																																																																																																																								local_4 = uVar9;
																																																																																																																																							}
																																																																																																																																						}
																																																																																																																																						else {
																																																																																																																																							if (uVar9 == 0x8c) {
																																																																																																																																								if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																									FUN_2e48_d15c(1, uVar25, (char)*(undefined2
																																																																																																																																										*)0x5d0,
																																																																																																																																										*(undefined*)
																																																																																																																																										(iVar17 + -0x7dbf));
																																																																																																																																									*(uint*)(iVar17 + -0x7da6) =
																																																																																																																																										*(int*)(iVar17 + -0x7db0) * param_1
																																																																																																																																										+ *(int*)(iVar17 + -0x7da6) & 0x1ff;
																																																																																																																																									local_4 = *(int*)(iVar17 + -0x7db2) *
																																																																																																																																										param_1 + *(int*)(iVar17 +
																																																																																																																																											-0x7da8) & 0x1ff;
																																																																																																																																									*(uint*)(iVar17 + -0x7da8) = local_4;
																																																																																																																																								}
																																																																																																																																							}
																																																																																																																																							else {
																																																																																																																																								if (uVar9 == 0x8d) {
																																																																																																																																									if (0 < *(int*)(iVar17 + -0x7d96)) {
																																																																																																																																										*(uint*)0x5cc = local_6;
																																																																																																																																										local_4 = local_6;
																																																																																																																																									}
																																																																																																																																								}
																																																																																																																																								else {
																																																																																																																																									if (uVar9 == 0x8e) {
																																																																																																																																										FUN_2e48_0078((char)*(undefined2*)
																																																																																																																																											(iVar17 + -0x7d9e));
																																																																																																																																										bStack69 = (byte)((uint)uVar19 >> 8);
																																																																																																																																										iVar18 = (int)((ulong)(CONCAT13(bStack69,
																																																																																																																																											CONCAT12((char)uVar19, uVar9)) * 0x32) >> 0x10) +
																																																																																																																																											0x32;
																																																																																																																																										*(int*)(iVar17 + -0x7d9a) = iVar18;
																																																																																																																																										FUN_3ce4_170a(0x32);
																																																																																																																																										if (iVar18 == 0) {
																																																																																																																																											FUN_3ce4_170a(7);
																																																																																																																																											iVar18 = iVar18 + 2;
																																																																																																																																											*(int*)(iVar17 + -0x7db2) = iVar18;
																																																																																																																																										}
																																																																																																																																										FUN_3ce4_170a(100);
																																																																																																																																										if (iVar18 == 0) {
																																																																																																																																											FUN_3ce4_170a(2);
																																																																																																																																											*(int*)(iVar17 + -0x7da2) = iVar18 << 8;
																																																																																																																																										}
																																																																																																																																										local_4 = *(int*)(iVar17 + -0x7db2) * param_1;
																																																																																																																																										FUN_2e48_0a6c((char)local_4,
																																																																																																																																											(char)*(undefined2*)
																																																																																																																																											(iVar17 + -0x7da2),
																																																																																																																																											(char)*(undefined2*)
																																																																																																																																											(iVar17 + -0x7d9e));
																																																																																																																																										piVar1 = (int*)(iVar17 + -0x7d9e);
																																																																																																																																										*piVar1 = *piVar1 + local_4;
																																																																																																																																									}
																																																																																																																																									else {
																																																																																																																																										if (uVar9 == 0x8f) {
																																																																																																																																											FUN_2e48_cf8b(0, 7, (char)*(undefined2*)
																																																																																																																																												(iVar17 + -0x7db4),
																																																																																																																																												(char)*(undefined2*)
																																																																																																																																												(iVar17 + -0x7db8));
																																																																																																																																											if (cVar28 != '\0') {
																																																																																																																																												*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																																											}
																																																																																																																																											FUN_3ce4_170a((char)*(undefined2*)
																																																																																																																																												(iVar17 + -0x7db2) +
																																																																																																																																												'\x01');
																																																																																																																																											local_4 = param_1;
																																																																																																																																											FUN_3ce4_0e1c();
																																																																																																																																											FUN_3ce4_170a((char)local_4);
																																																																																																																																											piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																																																											*piVar1 = *piVar1 + local_4;
																																																																																																																																											if (100 < *(int*)(iVar17 + -0x7d9a)) {
																																																																																																																																												local_4 = 0;
																																																																																																																																												*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																																											}
																																																																																																																																											*(undefined*)(iVar17 + -0x7dbd) = 4;
																																																																																																																																											FUN_2e48_2b33(uVar25);
																																																																																																																																											if (0 < (int)local_4) {
																																																																																																																																												iVar18 = local_4 * 0x36;
																																																																																																																																												bVar6 = *(byte*)(iVar18 + -0x7dbe);
																																																																																																																																												local_4 = local_4 & 0xff00 | (uint)bVar6;
																																																																																																																																												if (bVar6 == 0x40) {
																																																																																																																																													*(undefined2*)(iVar18 + -0x7dbc) = 0x8f;
																																																																																																																																													*(undefined*)(iVar18 + -0x7dc1) = 1;
																																																																																																																																												}
																																																																																																																																												else {
																																																																																																																																													*(undefined*)(iVar18 + -0x7dbd) = 1;
																																																																																																																																												}
																																																																																																																																											}
																																																																																																																																											*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																																																																										}
																																																																																																																																										else {
																																																																																																																																											if (uVar9 == 0x90) {
																																																																																																																																												FUN_2e48_0a6c(1, (char)*(undefined2*)0x2b0e,
																																																																																																																																													(char)*(undefined2*)
																																																																																																																																													(iVar17 + -0x7d9a));
																																																																																																																																												uVar8 = uVar9;
																																																																																																																																												if (100 < *(int*)0x2b0e) {
																																																																																																																																													uVar8 = *(int*)(iVar17 + -0x7da2) + 1;
																																																																																																																																													*(uint*)(iVar17 + -0x7d9a) = uVar8;
																																																																																																																																												}
																																																																																																																																												FUN_3ce4_170a(5);
																																																																																																																																												if ((((int)param_1 < 0 && -1 < iVar18) ||
																																																																																																																																													((-1 < iVar18 && (uVar8 < param_1)))) &&
																																																																																																																																													(0 < (int)uVar9)) {
																																																																																																																																													piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																																																													*piVar1 = *piVar1 + uVar9;
																																																																																																																																													uVar8 = uVar9;
																																																																																																																																													FUN_3ce4_170a(2);
																																																																																																																																													if (uVar8 == 0) {
																																																																																																																																														FUN_2e48_d03f(0x55);
																																																																																																																																													}
																																																																																																																																													uVar7 = (undefined)uVar8;
																																																																																																																																													uVar23 = 99;
																																																																																																																																													FUN_2e48_d001(0);
																																																																																																																																													FUN_2e48_d15c(2, uVar25, uVar7, uVar23);
																																																																																																																																												}
																																																																																																																																												local_4 = *(uint*)(iVar17 + -0x7d9a);
																																																																																																																																												local_16 = uVar9;
																																																																																																																																												if (*(int*)(iVar17 + -0x7da2) <=
																																																																																																																																													(int)local_4) {
																																																																																																																																													local_4 = 0;
																																																																																																																																													*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																																													*(undefined*)(iVar17 + -0x7dc1) = 2;
																																																																																																																																													*(undefined*)(iVar17 + -0x7dc2) = 0;
																																																																																																																																													FUN_2e48_d544();
																																																																																																																																													FUN_2e48_d20c();
																																																																																																																																													uVar7 = 0x4c;
																																																																																																																																													FUN_2e48_d001(0);
																																																																																																																																													FUN_2e48_d15c(3, uVar25, (char)local_4, uVar7
																																																																																																																																													);
																																																																																																																																													uVar7 = 0x51;
																																																																																																																																													FUN_2e48_d001(0);
																																																																																																																																													FUN_2e48_d15c(3, uVar25, (char)local_4, uVar7
																																																																																																																																													);
																																																																																																																																													uVar7 = 0x20;
																																																																																																																																													FUN_2e48_d001(0);
																																																																																																																																													FUN_2e48_d15c(3, uVar25, (char)local_4, uVar7
																																																																																																																																													);
																																																																																																																																												}
																																																																																																																																											}
																																																																																																																																											else {
																																																																																																																																												if (uVar9 == 0x91) {
																																																																																																																																													*(undefined*)(iVar17 + -0x7dbe) = 0x5a;
																																																																																																																																													FUN_2e48_00b5(0x10, 1, cVar27 + '^', uVar24);
																																																																																																																																													*(undefined*)(iVar17 + -0x7dc0) = 2;
																																																																																																																																													if (*(int*)(iVar17 + -0x7d96) ==
																																																																																																																																														*(int*)(iVar17 + -0x7da2)) {
																																																																																																																																														*(undefined*)
																																																																																																																																															(*(int*)(iVar17 + -0x7da2) + 0x5d6) =
																																																																																																																																															0x32;
																																																																																																																																													}
																																																																																																																																													else {
																																																																																																																																														*(undefined*)
																																																																																																																																															(*(int*)(iVar17 + -0x7da2) + 0x5d6) =
																																																																																																																																															0x31;
																																																																																																																																													}
																																																																																																																																													if (0 < *(int*)(iVar17 + -0x7d9c)) {
																																																																																																																																														FUN_20ac_befc(uVar25);
																																																																																																																																													}
																																																																																																																																													*(undefined2*)(iVar17 + -0x7d9c) = 0;
																																																																																																																																													local_4 = 0;
																																																																																																																																												}
																																																																																																																																												else {
																																																																																																																																													if (uVar9 == 0x92) {
																																																																																																																																														*(undefined*)(iVar17 + -0x7dc0) = 3;
																																																																																																																																														uVar7 = *(undefined*)(iVar17 + -0x7dbf)
																																																																																																																																															;
																																																																																																																																														uVar23 = uVar7;
																																																																																																																																														FUN_2e48_d001(0);
																																																																																																																																														FUN_2e48_d15c(1, uVar25, uVar23, uVar7);
																																																																																																																																														iVar18 = param_1 << 3;
																																																																																																																																														FUN_2e48_0a6c((char)iVar18,
																																																																																																																																															(char)*(undefined2*)
																																																																																																																																															(iVar17 + -0x7db2),
																																																																																																																																															0);
																																																																																																																																														piVar1 = (int*)(iVar17 + -0x7db8);
																																																																																																																																														*piVar1 = *piVar1 + iVar18;
																																																																																																																																														piVar1 = (int*)(iVar17 + -0x7db2);
																																																																																																																																														*piVar1 = *piVar1 - iVar18;
																																																																																																																																														local_16 = param_1 << 3;
																																																																																																																																														FUN_2e48_0a6c((char)local_16,
																																																																																																																																															(char)*(undefined2*)
																																																																																																																																															(iVar17 + -0x7dae),
																																																																																																																																															0);
																																																																																																																																														piVar1 = (int*)(iVar17 + -0x7db4);
																																																																																																																																														*piVar1 = *piVar1 + local_16;
																																																																																																																																														piVar1 = (int*)(iVar17 + -0x7dae);
																																																																																																																																														*piVar1 = *piVar1 - local_16;
																																																																																																																																														*(undefined*)
																																																																																																																																															(*(int*)(iVar17 + -0x7da2) + 0x5d6) =
																																																																																																																																															0x31;
																																																																																																																																														if ((*(int*)(iVar17 + -0x7db2) == 0) &&
																																																																																																																																															(*(int*)(iVar17 + -0x7dae) == 0)) {
																																																																																																																																															*(undefined2*)(iVar17 + -0x7dbc) =
																																																																																																																																																0x91;
																																																																																																																																															*(undefined*)
																																																																																																																																																(*(int*)(iVar17 + -0x7da0) + 0x5d6)
																																																																																																																																																= 0x30;
																																																																																																																																														}
																																																																																																																																														*(undefined2*)(iVar17 + -0x7d9c) = 0;
																																																																																																																																														local_4 = 0;
																																																																																																																																													}
																																																																																																																																													else {
																																																																																																																																														if (uVar9 == 0x93) {
																																																																																																																																															uVar9 = 0;
																																																																																																																																															FUN_3ce4_1b8c(*(undefined*)
																																																																																																																																																(iVar17 + -0x7d96), 0x15
																																																																																																																																																, 200, unaff_SS);
																																																																																																																																															local_4 = uVar9 & 0xff00 |
																																																																																																																																																(uint) * (byte*)(iVar17 +
																																																																																																																																																	-0x7d90);
																																																																																																																																															FUN_3ce4_0f85(0xd6, uVar24, 200, unaff_SS);
																																																																																																																																															if (0 < (int)local_4) {
																																																																																																																																																FUN_20ac_2094(uVar25);
																																																																																																																																																*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																																																local_4 = 0;
																																																																																																																																															}
																																																																																																																																														}
																																																																																																																																														else {
																																																																																																																																															if (uVar9 == 0x94) {
																																																																																																																																																*(undefined*)(iVar17 + -0x7dbd) = 1;
																																																																																																																																																FUN_2e48_2b33(uVar25);
																																																																																																																																																*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																																																																																local_4 = uVar9;
																																																																																																																																																local_16 = uVar9;
																																																																																																																																																if (((0 < (int)uVar9) &&
																																																																																																																																																	(*(char*)(uVar9 * 0x36 + -0x7dbe) == 'Z')
																																																																																																																																																	) && (local_4 = *(uint*)(uVar9 * 0x36 +
																																																																																																																																																		-0x7d96),
																																																																																																																																																		local_4 ==
																																																																																																																																																		*(uint*)(iVar17 + -0x7d96))) {
																																																																																																																																																	FUN_20ac_2094(uVar25);
																																																																																																																																																	*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																																																	*(undefined*)(iVar17 + -0x7dbe) = 0;
																																																																																																																																																	local_4 = 0;
																																																																																																																																																}
																																																																																																																																															}
																																																																																																																																															else {
																																																																																																																																																if (uVar9 == 0x95) {
																																																																																																																																																	if ((*(int*)(iVar17 + -0x7d9e) == 0) &&
																																																																																																																																																		(0 < *(int*)(iVar17 + -0x7db2))) {
																																																																																																																																																		piVar1 = (int*)(iVar17 + -0x7d9e);
																																																																																																																																																		*piVar1 = *piVar1 + *(int*)(iVar17 +
																																																																																																																																																			-0x7db2) * *(int*)(iVar17 + -0x7da2);
																																																																																																																																																		FUN_2e48_d15c(1, uVar25, (char)*(undefined2*)0x5d0,
																																																																																																																																																			*(undefined*)(iVar17 + -0x7dbf));
																																																																																																																																																	}
																																																																																																																																																	iVar18 = 0;
																																																																																																																																																	*(undefined2*)(iVar17 + -0x7db2) = 0;
																																																																																																																																																	FUN_2e48_0a6c(5, 0, (char)*(undefined2*)
																																																																																																																																																		(iVar17 + -0x7d9e));
																																																																																																																																																	*(uint*)(iVar17 + -0x7da8) =
																																																																																																																																																		*(int*)(iVar17 + -0x7da8) + iVar18 & 0x1ff;
																																																																																																																																																	piVar1 = (int*)(iVar17 + -0x7d9e);
																																																																																																																																																	*piVar1 = *piVar1 + iVar18;
																																																																																																																																																	uVar9 = (uint) * (byte*)(*(int*)(iVar17 + -0x7d98)
																																																																																																																																																		+ -0x1390);
																																																																																																																																																	if (*(int*)(iVar17 + -0x7da8) ==
																																																																																																																																																		*(int*)(iVar17 + -0x7da0)) {
																																																																																																																																																		local_16 = ~*(uint*)(iVar17 + -0x7dae) & uVar9;
																																																																																																																																																	}
																																																																																																																																																	else {
																																																																																																																																																		local_16 = uVar9 | *(uint*)(iVar17 + -0x7dae);
																																																																																																																																																	}
																																																																																																																																																	*(undefined*)
																																																																																																																																																		(*(int*)(iVar17 + -0x7d98) + -0x1390) =
																																																																																																																																																		(char)(local_16 & 0xff);
																																																																																																																																																	local_4 = local_16 & 0xff;
																																																																																																																																																}
																																																																																																																																																else {
																																																																																																																																																	if (uVar9 == 0x96) {
																																																																																																																																																		FUN_2e48_2d63(uVar25);
																																																																																																																																																		local_4 = uVar9;
																																																																																																																																																		if ((char)uVar9 != '\0') {
																																																																																																																																																			*(undefined2*)0xdc4c =
																																																																																																																																																				*(undefined2*)(iVar17 + -0x7da6);
																																																																																																																																																			*(undefined2*)(iVar17 + -0x7dbc) = 0x97;
																																																																																																																																																			*(undefined2*)0x8294 = 0x41;
																																																																																																																																																			*(undefined2*)0xd4d8 = 0;
																																																																																																																																																			*(undefined2*)0xd4da = 0;
																																																																																																																																																			*(undefined2*)0xd4b6 = 10;
																																																																																																																																																			local_4 = 0;
																																																																																																																																																		}
																																																																																																																																																	}
																																																																																																																																																	else {
																																																																																																																																																		if (uVar9 == 0x97) {
																																																																																																																																																			FUN_20ac_ce41(cVar22, uVar25);
																																																																																																																																																			local_4 = uVar9;
																																																																																																																																																		}
																																																																																																																																																		else {
																																																																																																																																																			if (uVar9 == 0x98) {
																																																																																																																																																				local_4 = *(uint*)0x2b0e;
																																																																																																																																																				*(uint*)(iVar17 + -0x7d9a) = local_4;
																																																																																																																																																			}
																																																																																																																																																			else {
																																																																																																																																																				if (uVar9 == 0x99) {
																																																																																																																																																					if (*(int*)(iVar17 + -0x7dba) == 1000)
																																																																																																																																																					{
																																																																																																																																																						uVar9 = *(uint*)(iVar17 + -0x7dae);
																																																																																																																																																						*(uint*)0xd494 = uVar9;
																																																																																																																																																						FUN_3ce4_170a(100);
																																																																																																																																																						uVar8 = *(int*)(iVar17 + -0x7db2) *
																																																																																																																																																							param_1;
																																																																																																																																																						if (((int)uVar8 < 0 &&
																																																																																																																																																							-1 < (int)uVar8 >> 0xf) ||
																																																																																																																																																							((-1 < (int)uVar8 >> 0xf &&
																																																																																																																																																								(uVar9 < uVar8)))) {
																																																																																																																																																							FUN_2e48_0a6c(1, (char)*(undefined2*
																																																																																																																																																								)(iVar17 +
																																																																																																																																																									-0x7da0), (char)*(undefined2*)(iVar17 + -0x7dae));
																																																																																																																																																							piVar1 = (int*)(iVar17 + -0x7dae);
																																																																																																																																																							*piVar1 = *piVar1 + uVar8;
																																																																																																																																																						}
																																																																																																																																																						iVar18 = *(int*)(iVar17 + -0x7dae);
																																																																																																																																																						if ((iVar18 != *(int*)(iVar17 + -0x7da0)) &&
																																																																																																																																																							(FUN_3ce4_170a(0xf), iVar18 == 0)) {
																																																																																																																																																							FUN_2e48_d15c(2, uVar25, (char)*(undefined2*)
																																																																																																																																																								0x5d0, 0xe);
																																																																																																																																																						}
																																																																																																																																																						*(undefined2*)0x2de7 = 0;
																																																																																																																																																						local_4 = 0;
																																																																																																																																																					}
																																																																																																																																																					else {
																																																																																																																																																						local_4 = *(uint*)(iVar17 + -0x7da2);
																																																																																																																																																						*(uint*)(iVar17 + -0x7dae) = local_4;
																																																																																																																																																					}
																																																																																																																																																				}
																																																																																																																																																				else {
																																																																																																																																																					if (uVar9 == 0x9a) {
																																																																																																																																																						if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																							FUN_20ac_b2c8(*(undefined*)
																																																																																																																																																								(iVar17 + -0x7da0),
																																																																																																																																																								*(undefined*)
																																																																																																																																																								(iVar17 + -0x7da2),
																																																																																																																																																								(char)*(undefined2*)
																																																																																																																																																								(iVar17 + -0x7db4),
																																																																																																																																																								(char)*(undefined2*)
																																																																																																																																																								(iVar17 + -0x7db8));
																																																																																																																																																							*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																																																							local_4 = 0;
																																																																																																																																																						}
																																																																																																																																																					}
																																																																																																																																																					else {
																																																																																																																																																						if (uVar9 == 0x9b) {
																																																																																																																																																							FUN_20ac_bfe7(cVar22, uVar25);
																																																																																																																																																							local_4 = uVar9;
																																																																																																																																																						}
																																																																																																																																																						else {
																																																																																																																																																							if (uVar9 == 0x9c) {
																																																																																																																																																								if ((0 < *(int*)(iVar17 + -0x7d96)) &&
																																																																																																																																																									(0 < *(int*)(iVar17 + -0x7da2))) {
																																																																																																																																																									local_4 = *(int*)(*(int*)(iVar17 +
																																																																																																																																																										-0x7d96) * 0x36 + -0x7da2) * 0x200;
																																																																																																																																																									FUN_3ce4_0e37();
																																																																																																																																																									*(uint*)(iVar17 + -0x7da4) = local_4;
																																																																																																																																																								}
																																																																																																																																																							}
																																																																																																																																																							else {
																																																																																																																																																								if (uVar9 == 0x9d) {
																																																																																																																																																									lVar5 = (ulong)param_1 *
																																																																																																																																																										(ulong) * (uint*)(iVar17 + -0x7da2);
																																																																																																																																																									uVar13 = (undefined2)((ulong)lVar5 >> 0x10);
																																																																																																																																																									uVar9 = (int)lVar5 +
																																																																																																																																																										*(int*)(iVar17 + -0x7dae) & 0x1ff;
																																																																																																																																																									*(uint*)(iVar17 + -0x7dae) = uVar9;
																																																																																																																																																									FUN_2e48_0051((char)*(undefined2*)
																																																																																																																																																										(iVar17 + -0x7dae));
																																																																																																																																																									bStack69 = (byte)((uint)uVar13 >> 8);
																																																																																																																																																									uVar19 = *(undefined2*)(iVar17 + -0x7da0);
																																																																																																																																																									cVar22 = (char)((uint)uVar19 >> 8);
																																																																																																																																																									local_4 = (uint)((ulong)(CONCAT13(bStack69,
																																																																																																																																																										CONCAT12((char)uVar13, uVar9)) *
																																																																																																																																																										CONCAT13(cVar22 >> 0xf,
																																																																																																																																																											CONCAT12(cVar22 >> 7, uVar19))) >> 0x10);
																																																																																																																																																									*(uint*)(iVar17 + -0x7da4) = local_4;
																																																																																																																																																								}
																																																																																																																																																								else {
																																																																																																																																																									if (uVar9 == 0x9e) {
																																																																																																																																																										piVar1 = (int*)(iVar17 + -0x7da2);
																																																																																																																																																										*piVar1 = *piVar1 + param_1;
																																																																																																																																																										local_4 = *(uint*)(iVar17 + -0x7da2);
																																																																																																																																																										if (*(int*)(iVar17 + -0x7da0) < (int)local_4)
																																																																																																																																																										{
																																																																																																																																																											*(undefined2*)(iVar17 + -0x7da2) = 0;
																																																																																																																																																											bVar6 = *(byte*)(iVar17 + -0x7dbf);
																																																																																																																																																											local_4 = (uint)bVar6;
																																																																																																																																																											FUN_2e48_d001(0);
																																																																																																																																																											FUN_2e48_d15c(1, uVar25, (char)local_4, bVar6);
																																																																																																																																																										}
																																																																																																																																																									}
																																																																																																																																																									else {
																																																																																																																																																										if (uVar9 == 0x9f) {
																																																																																																																																																											if ((*(int*)(iVar17 + -0x7dba) == 1000) &&
																																																																																																																																																												(local_4 = 1, 0 < (int)param_1)) {
																																																																																																																																																												local_4 = 1;
																																																																																																																																																												uVar9 = param_1;
																																																																																																																																																												while (true) {
																																																																																																																																																													*(uint*)(iVar17 + -0x7da6) =
																																																																																																																																																														*(int*)(iVar17 + -0x7da6) +
																																																																																																																																																														*(int*)(iVar17 + -0x7db2) & 0x1ff;
																																																																																																																																																													FUN_2e48_d15c(1, uVar25, (char)*(
																																																																																																																																																														undefined2*)0x5d0,
																																																																																																																																																														*(undefined*)(iVar17 + -0x7dbf));
																																																																																																																																																													piVar1 = (int*)(iVar17 + -0x7d9e);
																																																																																																																																																													*piVar1 = *piVar1 + -1;
																																																																																																																																																													if (*(int*)(iVar17 + -0x7d9e) < 1) {
																																																																																																																																																														*(undefined2*)(iVar17 + -0x7d9e) =
																																																																																																																																																															*(undefined2*)(iVar17 + -0x7da0);
																																																																																																																																																														*(int*)(iVar17 + -0x7dba) =
																																																																																																																																																															*(int*)(iVar17 + -0x7d96) + 1000;
																																																																																																																																																													}
																																																																																																																																																													if (local_4 == uVar9) break;
																																																																																																																																																													local_4 = local_4 + 1;
																																																																																																																																																												}
																																																																																																																																																											}
																																																																																																																																																										}
																																																																																																																																																										else {
																																																																																																																																																											if (uVar9 == 0xa1) {
																																																																																																																																																												FUN_3ce4_170a(0);
																																																																																																																																																												*(uint*)(iVar17 + -0x7db0) = uVar9;
																																																																																																																																																												local_4 = uVar9;
																																																																																																																																																											}
																																																																																																																																																											else {
																																																																																																																																																												if (uVar9 == 0xa2) {
																																																																																																																																																													if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																														FUN_2e48_d15c(1, uVar25, (char)*(undefined2
																																																																																																																																																															*)0x5d0,
																																																																																																																																																															*(undefined*)
																																																																																																																																																															(iVar17 + -0x7dbf));
																																																																																																																																																														FUN_20ac_4106(*(undefined2*)
																																																																																																																																																															(iVar17 + -0x7d96),
																																																																																																																																																															*(undefined2*)
																																																																																																																																																															(iVar17 + -0x7da6),
																																																																																																																																																															(char)*(undefined2*)
																																																																																																																																																															(iVar17 + -0x7db4),
																																																																																																																																																															(char)*(undefined2*)
																																																																																																																																																															(iVar17 + -0x7db6),
																																																																																																																																																															(char)*(undefined2*)
																																																																																																																																																															(iVar17 + -0x7db8),
																																																																																																																																																															(char)*(undefined2*)
																																																																																																																																																															(iVar17 + -0x7da0),
																																																																																																																																																															uVar25);
																																																																																																																																																														*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																																																														local_4 = 0;
																																																																																																																																																														if (*(char*)(iVar17 + -0x7dc2) != '\0') {
																																																																																																																																																															bVar6 = *(byte*)(iVar17 + -0x7dc2);
																																																																																																																																																															*(byte*)(iVar17 + -0x7dc1) = bVar6;
																																																																																																																																																															local_4 = (uint)bVar6;
																																																																																																																																																														}
																																																																																																																																																													}
																																																																																																																																																												}
																																																																																																																																																												else {
																																																																																																																																																													if (uVar9 == 0xa3) {
																																																																																																																																																														if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																															if ((*(int*)0x5d2 <
																																																																																																																																																																*(int*)(iVar17 + -0x7dae)) ||
																																																																																																																																																																(0 < *(int*)(iVar17 + -0x7da8))) {
																																																																																																																																																																*(int*)(iVar17 + -0x7da0) =
																																																																																																																																																																	(int)(*(int*)(iVar17 + -0x7db2)
																																																																																																																																																																		* param_1 +
																																																																																																																																																																		*(int*)(iVar17 + -0x7da0))
																																																																																																																																																																	% *(int*)(iVar17 + -0x7da2);
																																																																																																																																																															}
																																																																																																																																																															if (*(int*)(iVar17 + -0x7da0) <
																																																																																																																																																																*(int*)(iVar17 + -0x7db0) << 1) {
																																																																																																																																																																if (*(int*)(iVar17 + -0x7da8) == 0) {
																																																																																																																																																																	FUN_2e48_d15c(1, uVar25, (char)*(
																																																																																																																																																																		undefined2*)0x5d0,
																																																																																																																																																																		*(undefined*)(iVar17 + -0x7dbf));
																																																																																																																																																																}
																																																																																																																																																																uVar9 = *(int*)(iVar17 + -0x7da0) -
																																																																																																																																																																	*(int*)(iVar17 + -0x7db0);
																																																																																																																																																																uVar8 = (int)uVar9 >> 0xf;
																																																																																																																																																																local_4 = *(int*)(iVar17 + -0x7db0) -
																																																																																																																																																																	((uVar9 ^ uVar8) - uVar8);
																																																																																																																																																																*(uint*)(iVar17 + -0x7da8) = local_4;
																																																																																																																																																																FUN_2e48_00e4((char)*(undefined2*)
																																																																																																																																																																	(iVar17 + -0x7db0), 10,
																																																																																																																																																																	(char)*(undefined2*)
																																																																																																																																																																	(iVar17 + -0x7da0));
																																																																																																																																																																if (((char)local_4 == '\0') &&
																																																																																																																																																																	(FUN_2e48_2d63(uVar25), (char)local_4 != '\0'))
																																																																																																																																																																{
																																																																																																																																																																	FUN_20ac_2124(uVar25);
																																																																																																																																																																}
																																																																																																																																																															}
																																																																																																																																																															else {
																																																																																																																																																																*(undefined2*)(iVar17 + -0x7da8) = 0;
																																																																																																																																																																*(undefined2*)(iVar17 + -0x7d9e) = 0;
																																																																																																																																																																local_4 = 0;
																																																																																																																																																															}
																																																																																																																																																														}
																																																																																																																																																													}
																																																																																																																																																													else {
																																																																																																																																																														if (uVar9 == 0xa4) {
																																																																																																																																																															FUN_20ac_d90b(cVar22, uVar25);
																																																																																																																																																															local_4 = uVar9;
																																																																																																																																																														}
																																																																																																																																																														else {
																																																																																																																																																															if (uVar9 == 0xa5) {
																																																																																																																																																																*(undefined*)(iVar17 + -0x7dbe) =
																																																																																																																																																																	*(undefined*)(iVar17 + -0x7db2);
																																																																																																																																																																iVar18 = *(int*)(iVar17 + -0x7daa);
																																																																																																																																																																iVar12 = iVar18 >> 1;
																																																																																																																																																																uVar7 = (undefined)iVar12;
																																																																																																																																																																bStack69 = (byte)(iVar18 >> 9);
																																																																																																																																																																FUN_2e48_18cf((char)*(undefined2*)
																																																																																																																																																																	(iVar17 + -0x7db4),
																																																																																																																																																																	(char)*(undefined2*)
																																																																																																																																																																	(iVar17 + -0x7db6),
																																																																																																																																																																	(char)*(undefined2*)
																																																																																																																																																																	(iVar17 + -0x7db8));
																																																																																																																																																																*(int*)(iVar17 + -0x7db6) =
																																																																																																																																																																	iVar12 + CONCAT11(bStack69, uVar7);
																																																																																																																																																																*(int*)(iVar17 + -0x7db8) =
																																																																																																																																																																	((*(int*)(iVar17 + -0x7db8) + -0xc) /
																																																																																																																																																																		0x32) * 0x32 + 0x19;
																																																																																																																																																																*(int*)(iVar17 + -0x7db4) =
																																																																																																																																																																	((*(int*)(iVar17 + -0x7db4) + -0xc) /
																																																																																																																																																																		0x32) * 0x32 + 0x19;
																																																																																																																																																																FUN_2e48_2ec4(0xea, unaff_SS,
																																																																																																																																																																	(char)*(undefined2*)
																																																																																																																																																																	(iVar17 + -0x7db4) -
																																																																																																																																																																	(char)*(undefined2*)0xd4d6,
																																																																																																																																																																	(char)*(undefined2*)
																																																																																																																																																																	(iVar17 + -0x7db8) -
																																																																																																																																																																	(char)*(undefined2*)0xd4d2);
																																																																																																																																																																local_16 = 0xcc - ((int)(local_16 & 0xf) >>
																																																																																																																																																																	1);
																																																																																																																																																																local_4 = local_16;
																																																																																																																																																																FUN_2e48_00b5(0xca, 0xc1, 0xea, unaff_SS);
																																																																																																																																																																if (0 < *(int*)(iVar17 + -0x7dae)) {
																																																																																																																																																																	iVar18 = (*pbVar29 - 0x1e) * 0x1a4;
																																																																																																																																																																	local_4 = *(uint*)(iVar18 + 0x7048) |
																																																																																																																																																																		*(uint*)(iVar18 + 0x704a);
																																																																																																																																																																	if (local_4 != 0) {
																																																																																																																																																																		uVar4 = *(undefined4*)(iVar18 + 0x7048)
																																																																																																																																																																			;
																																																																																																																																																																		*(undefined*)((int)uVar4 + 4) =
																																																																																																																																																																			(char)(local_16 & 0xff);
																																																																																																																																																																		local_4 = local_4 & 0xff00 |
																																																																																																																																																																			local_16 & 0xff;
																																																																																																																																																																	}
																																																																																																																																																																}
																																																																																																																																																															}
																																																																																																																																																															else {
																																																																																																																																																																if (uVar9 == 0xa6) {
																																																																																																																																																																	FUN_20ac_c7c5(cVar27 + 'J', uVar24, cVar22,
																																																																																																																																																																		uVar25);
																																																																																																																																																																	local_4 = uVar9;
																																																																																																																																																																}
																																																																																																																																																																else {
																																																																																																																																																																	if (uVar9 == 0xa7) {
																																																																																																																																																																		FUN_20ac_c7c5(cVar27 + 'H', uVar24, cVar22
																																																																																																																																																																			, uVar25);
																																																																																																																																																																		local_4 = uVar9;
																																																																																																																																																																	}
																																																																																																																																																																	else {
																																																																																																																																																																		if (uVar9 == 0xa8) {
																																																																																																																																																																			FUN_20ac_c7c5(cVar27 + 'L', uVar24,
																																																																																																																																																																				cVar22, uVar25);
																																																																																																																																																																			local_4 = uVar9;
																																																																																																																																																																		}
																																																																																																																																																																		else {
																																																																																																																																																																			if (uVar9 == 0xa9) {
																																																																																																																																																																				if ((*(int*)(iVar17 + -0x7dba) ==
																																																																																																																																																																					1000) &&
																																																																																																																																																																					(piVar1 = (int*)(iVar17 +
																																																																																																																																																																						-0x7da0), *piVar1 = *piVar1 - param_1,
																																																																																																																																																																						local_4 = param_1, *(int*)(iVar17 + -0x7da0) < 0)
																																																																																																																																																																					) {
																																																																																																																																																																					uVar7 = *(undefined*)(iVar17 + -0x7dbf);
																																																																																																																																																																					uVar23 = uVar7;
																																																																																																																																																																					FUN_2e48_d001(0);
																																																																																																																																																																					FUN_2e48_d15c(1, 0x59, uVar23, uVar7);
																																																																																																																																																																					FUN_20ac_2060((char)*(undefined2*)
																																																																																																																																																																						(iVar17 + -0x7da2));
																																																																																																																																																																					*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																																																																					local_4 = 0;
																																																																																																																																																																				}
																																																																																																																																																																			}
																																																																																																																																																																			else {
																																																																																																																																																																				if (uVar9 == 0xaa) {
																																																																																																																																																																					if ((*(int*)(iVar17 + -0x7dba) == 1000) &&
																																																																																																																																																																						(local_4 = 1, 0 < (int)param_1)) {
																																																																																																																																																																						local_4 = 1;
																																																																																																																																																																						uVar9 = param_1;
																																																																																																																																																																						while (FUN_20ac_bcae(uVar25),
																																																																																																																																																																							local_4 != uVar9) {
																																																																																																																																																																							local_4 = local_4 + 1;
																																																																																																																																																																						}
																																																																																																																																																																					}
																																																																																																																																																																				}
																																																																																																																																																																				else {
																																																																																																																																																																					if (uVar9 == 0xab) {
																																																																																																																																																																						if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																																							piVar1 = (int*)(iVar17 + -0x7da2);
																																																																																																																																																																							*piVar1 = *piVar1 - param_1;
																																																																																																																																																																							*(undefined*)(iVar17 + -0x7d91) = 4;
																																																																																																																																																																							uVar9 = param_1;
																																																																																																																																																																						}
																																																																																																																																																																						local_4 = uVar9;
																																																																																																																																																																						if (0 < *(int*)(iVar17 + -0x7da2)) {
																																																																																																																																																																							local_4 = *(int*)(iVar17 + -0x7da2) >> 1;
																																																																																																																																																																							*(uint*)0x2b0a = local_4;
																																																																																																																																																																						}
																																																																																																																																																																					}
																																																																																																																																																																					else {
																																																																																																																																																																						if (uVar9 == 0xac) {
																																																																																																																																																																							if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																																								FUN_20ac_cd75(cVar22, uVar25);
																																																																																																																																																																								local_4 = uVar9;
																																																																																																																																																																							}
																																																																																																																																																																						}
																																																																																																																																																																						else {
																																																																																																																																																																							if (uVar9 == 0xad) {
																																																																																																																																																																								iVar18 = *(int*)(iVar17 + -0x7daa);
																																																																																																																																																																								iVar12 = iVar18 >> 1;
																																																																																																																																																																								uVar7 = (undefined)iVar12;
																																																																																																																																																																								bStack69 = (byte)(iVar18 >> 9);
																																																																																																																																																																								FUN_2e48_18cf((char)*(undefined2*)
																																																																																																																																																																									(iVar17 + -0x7db4),
																																																																																																																																																																									(char)*(undefined2*)
																																																																																																																																																																									(iVar17 + -0x7db6),
																																																																																																																																																																									(char)*(undefined2*)
																																																																																																																																																																									(iVar17 + -0x7db8))
																																																																																																																																																																									;
																																																																																																																																																																								*(int*)(iVar17 + -0x7db6) =
																																																																																																																																																																									iVar12 + CONCAT11(bStack69, uVar7);
																																																																																																																																																																								local_4 = 0;
																																																																																																																																																																								*(undefined2*)0x2b0e = 0;
																																																																																																																																																																								if (-1 < *(int*)(iVar17 + -0x7da8)) {
																																																																																																																																																																									local_4 = 0;
																																																																																																																																																																									FUN_2e48_0a6c(0xc, 0x80, (char)*(
																																																																																																																																																																										undefined2*)(iVar17 + -0x7da8));
																																																																																																																																																																									piVar1 = (int*)(iVar17 + -0x7da8);
																																																																																																																																																																									*piVar1 = *piVar1 + local_4;
																																																																																																																																																																								}
																																																																																																																																																																								FUN_2e48_2b33(uVar25);
																																																																																																																																																																								local_16 = local_4;
																																																																																																																																																																								if ((0 < (int)local_4) &&
																																																																																																																																																																									(*(int*)(local_4 * 0x36 + -0x7dbc) == 0xac)) {
																																																																																																																																																																									*(undefined2*)0x2b0e = 1;
																																																																																																																																																																									*(undefined*)(iVar17 + -0x7dbd) = 4;
																																																																																																																																																																								}
																																																																																																																																																																							}
																																																																																																																																																																							else {
																																																																																																																																																																								if (uVar9 == 0xae) {
																																																																																																																																																																									if (0 < (int)param_1) {
																																																																																																																																																																										local_4 = 1;
																																																																																																																																																																										uVar9 = param_1;
																																																																																																																																																																										while (FUN_20ac_5dec((char)*(undefined2*)
																																																																																																																																																																											(iVar17 +
																																																																																																																																																																												-0x7da0), (char)*(undefined2*)(iVar17 + -0x7da2),
																																																																																																																																																																											uVar25), local_4 != uVar9) {
																																																																																																																																																																											local_4 = local_4 + 1;
																																																																																																																																																																										}
																																																																																																																																																																									}
																																																																																																																																																																									*(undefined2*)(iVar17 + -0x7da8) = 0;
																																																																																																																																																																									*(undefined2*)(iVar17 + -0x7da4) = 0;
																																																																																																																																																																									local_4 = 0;
																																																																																																																																																																								}
																																																																																																																																																																								else {
																																																																																																																																																																									if (uVar9 == 0xaf) {
																																																																																																																																																																										FUN_20ac_c88c(cVar22, uVar25);
																																																																																																																																																																										local_4 = uVar9;
																																																																																																																																																																									}
																																																																																																																																																																									else {
																																																																																																																																																																										if (uVar9 == 0xb0) {
																																																																																																																																																																											local_4 = param_1 << 3;
																																																																																																																																																																											FUN_2e48_0a6c((char)local_4, 0,
																																																																																																																																																																												(char)*(undefined2*)
																																																																																																																																																																												(iVar17 + -0x7d9a));
																																																																																																																																																																											piVar1 = (int*)(iVar17 + -0x7d9a);
																																																																																																																																																																											*piVar1 = *piVar1 + local_4;
																																																																																																																																																																										}
																																																																																																																																																																										else {
																																																																																																																																																																											if (uVar9 == 0xb1) {
																																																																																																																																																																												if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																																													FUN_2e48_0a6c(cVar22, (char)*(undefined2
																																																																																																																																																																														*)(iVar17 +
																																																																																																																																																																															-0x7dae),
																																																																																																																																																																														(char)*(undefined2*)
																																																																																																																																																																														(iVar17 + -0x7da4))
																																																																																																																																																																														;
																																																																																																																																																																													piVar1 = (int*)(iVar17 + -0x7da4);
																																																																																																																																																																													*piVar1 = *piVar1 + uVar9;
																																																																																																																																																																													local_4 = uVar9;
																																																																																																																																																																												}
																																																																																																																																																																											}
																																																																																																																																																																											else {
																																																																																																																																																																												if (uVar9 == 0xb2) {
																																																																																																																																																																													if (*(int*)0x5d2 <
																																																																																																																																																																														*(int*)(iVar17 + -0x7da2)) {
																																																																																																																																																																														local_4 = *(uint*)0xd4c6;
																																																																																																																																																																														*(uint*)0xd4c4 = local_4;
																																																																																																																																																																													}
																																																																																																																																																																													else {
																																																																																																																																																																														*(undefined2*)0xd4c4 = 0;
																																																																																																																																																																														local_4 = 0;
																																																																																																																																																																													}
																																																																																																																																																																												}
																																																																																																																																																																												else {
																																																																																																																																																																													if (uVar9 == 0xb4) {
																																																																																																																																																																														if (*(int*)(iVar17 + -0x7dba) == 1000
																																																																																																																																																																															) {
																																																																																																																																																																															FUN_20ac_d5b4(cVar22, uVar25);
																																																																																																																																																																															local_4 = uVar9;
																																																																																																																																																																														}
																																																																																																																																																																													}
																																																																																																																																																																													else {
																																																																																																																																																																														if (uVar9 == 0xb5) {
																																																																																																																																																																															*(undefined*)(iVar17 + -0x7dbd) = 0
																																																																																																																																																																																;
																																																																																																																																																																															FUN_3ce4_170a(0x32);
																																																																																																																																																																															if (uVar9 == 0) {
																																																																																																																																																																																FUN_3ce4_170a(100);
																																																																																																																																																																																*(uint*)(iVar17 + -0x7da2) =
																																																																																																																																																																																	uVar9;
																																																																																																																																																																																FUN_3ce4_170a(0xb);
																																																																																																																																																																																*(uint*)(iVar17 + -0x7da0) =
																																																																																																																																																																																	uVar9;
																																																																																																																																																																															}
																																																																																																																																																																															FUN_2e48_0a6c(cVar22, 0, (char)*(
																																																																																																																																																																																undefined2*)(iVar17 + -0x7da2));
																																																																																																																																																																															piVar1 = (int*)(iVar17 + -0x7da2);
																																																																																																																																																																															*piVar1 = *piVar1 + uVar9;
																																																																																																																																																																															FUN_2e48_0a6c(1, 0x32, (char)*(undefined2*)
																																																																																																																																																																																(iVar17 + -0x7da4));
																																																																																																																																																																															piVar1 = (int*)(iVar17 + -0x7da4);
																																																																																																																																																																															*piVar1 = *piVar1 + uVar9;
																																																																																																																																																																															local_4 = uVar9;
																																																																																																																																																																															if (0 < *(int*)(iVar17 + -0x7da2)) {
																																																																																																																																																																																local_4 = (int)(param_1 * *(int*)(iVar17 +
																																																																																																																																																																																	-0x7da0) + *(int*)(iVar17 + -0x7d9a)) % 200;
																																																																																																																																																																																*(uint*)(iVar17 + -0x7d9a) = local_4;
																																																																																																																																																																															}
																																																																																																																																																																														}
																																																																																																																																																																														else {
																																																																																																																																																																															if (uVar9 == 0xb6) {
																																																																																																																																																																																if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																																																	FUN_20ac_d45f(uVar25);
																																																																																																																																																																																	local_4 = uVar9;
																																																																																																																																																																																}
																																																																																																																																																																															}
																																																																																																																																																																															else {
																																																																																																																																																																																if (uVar9 == 0xb7) {
																																																																																																																																																																																	if (*(int*)(iVar17 + -0x7dba) < 0x3e9) {
																																																																																																																																																																																		FUN_3ce4_170a(4);
																																																																																																																																																																																		local_4 = uVar9 + 1;
																																																																																																																																																																																		*(undefined*)(iVar17 + -0x7dc1) =
																																																																																																																																																																																			(char)local_4;
																																																																																																																																																																																		do {
																																																																																																																																																																																			FUN_3ce4_170a(4);
																																																																																																																																																																																			*(undefined*)(iVar17 + -0x7dc2) =
																																																																																																																																																																																				(char)(local_4 + 1);
																																																																																																																																																																																			bVar6 = *(byte*)(iVar17 + -0x7dc1);
																																																																																																																																																																																			local_4 = local_4 + 1 & 0xff00 |
																																																																																																																																																																																				(uint)bVar6;
																																																																																																																																																																																		} while (bVar6 == *(byte*)(iVar17 +
																																																																																																																																																																																			-0x7dc2));
																																																																																																																																																																																		*(undefined*)(iVar17 + -0x7d91) = 0x15;
																																																																																																																																																																																		if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																																																			*(undefined2*)(iVar17 + -0x7dba) = 0x14;
																																																																																																																																																																																		}
																																																																																																																																																																																	}
																																																																																																																																																																																}
																																																																																																																																																																																else {
																																																																																																																																																																																	if (uVar9 == 0xb8) {
																																																																																																																																																																																		*(undefined*)(iVar17 + -0x7dbd) = 1;
																																																																																																																																																																																		FUN_2e48_2d63(uVar25);
																																																																																																																																																																																		local_4 = uVar9 & 0xff00;
																																																																																																																																																																																		if ((char)uVar9 == '\0') {
																																																																																																																																																																																			local_4 = local_4 + 1;
																																																																																																																																																																																		}
																																																																																																																																																																																		*(undefined*)0x2ddc = (char)local_4;
																																																																																																																																																																																		*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																																																																																																																	}
																																																																																																																																																																																	else {
																																																																																																																																																																																		if (uVar9 == 0xbe) {
																																																																																																																																																																																			if (*(char*)0xf6bf != '\0') {
																																																																																																																																																																																				FUN_3ce4_170a(0xe8);
																																																																																																																																																																																				local_4 = *(uint*)(iVar17 + -0x7d9c);
																																																																																																																																																																																				if (((int)local_4 < 0 &&
																																																																																																																																																																																					-1 < (int)local_4 >> 0xf) ||
																																																																																																																																																																																					((-1 < (int)local_4 >> 0xf &&
																																																																																																																																																																																						(uVar9 < local_4)))) {
																																																																																																																																																																																					bVar6 = *(byte*)(iVar17 + -0x7dbf);
																																																																																																																																																																																					local_4 = (uint)bVar6;
																																																																																																																																																																																					FUN_2e48_d15c((char)*(undefined2*)
																																																																																																																																																																																						(iVar17 + -0x7d96),
																																																																																																																																																																																						uVar25, (char)*(undefined2
																																																																																																																																																																																							*)0x5d0,
																																																																																																																																																																																						bVar6);
																																																																																																																																																																																				}
																																																																																																																																																																																			}
																																																																																																																																																																																		}
																																																																																																																																																																																		else {
																																																																																																																																																																																			if (uVar9 == 200) {
																																																																																																																																																																																				if (*(int*)(iVar17 + -0x7dba) == 1000) {
																																																																																																																																																																																					uVar9 = *(uint*)(iVar17 + -0x7db0);
																																																																																																																																																																																					piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																																																																																																					*piVar1 = *piVar1 + uVar9;
																																																																																																																																																																																					bVar6 = *(byte*)(iVar17 + -0x7da2);
																																																																																																																																																																																					*(byte*)(iVar17 + -0x7d91) = bVar6;
																																																																																																																																																																																					local_4 = uVar9 & 0xff00 | (uint)bVar6;
																																																																																																																																																																																				}
																																																																																																																																																																																				else {
																																																																																																																																																																																					*(undefined*)(iVar17 + -0x7d91) = 0xff;
																																																																																																																																																																																				}
																																																																																																																																																																																			}
																																																																																																																																																																																			else {
																																																																																																																																																																																				if (uVar9 == 0xca) {
																																																																																																																																																																																					FUN_20ac_d306(cVar22, uVar25);
																																																																																																																																																																																					local_4 = uVar9;
																																																																																																																																																																																				}
																																																																																																																																																																																				else {
																																																																																																																																																																																					if (uVar9 == 0xcc) {
																																																																																																																																																																																						*(undefined*)(iVar17 + -0x7dbd) = 0;
																																																																																																																																																																																						pcVar2 = (char*)(iVar17 + -0x7d8f);
																																																																																																																																																																																						*pcVar2 = *pcVar2 + cVar22 * '\x0f';
																																																																																																																																																																																						piVar1 = (int*)(iVar17 + -0x7db6);
																																																																																																																																																																																						*piVar1 = *piVar1 + param_1 * 2;
																																																																																																																																																																																						*(undefined*)(iVar17 + -0x7d91) = 1;
																																																																																																																																																																																						local_4 = *(uint*)(iVar17 + -0x7dba);
																																																																																																																																																																																						*(uint*)(iVar17 + -0x7d9a) = local_4;
																																																																																																																																																																																					}
																																																																																																																																																																																					else {
																																																																																																																																																																																						if (uVar9 == 0xcd) {
																																																																																																																																																																																							bVar6 = *(byte*)(iVar17 + -0x7dbe);
																																																																																																																																																																																							if (bVar6 == 0x14) {
																																																																																																																																																																																								if (*(int*)(*(int*)(iVar17 +
																																																																																																																																																																																									-0x7da2) * 2 + -0x22ca) <
																																																																																																																																																																																									*(int*)(iVar17 + -0x7d9c)) {
																																																																																																																																																																																									*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																																																																																								}
																																																																																																																																																																																								else {
																																																																																																																																																																																									*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																																																																																								}
																																																																																																																																																																																								local_4 = 0;
																																																																																																																																																																																							}
																																																																																																																																																																																							else {
																																																																																																																																																																																								local_4 = (uint)bVar6;
																																																																																																																																																																																								if (bVar6 == 0x15) {
																																																																																																																																																																																									if (*(int*)(*(int*)(iVar17 + -0x7da2) * 2 +
																																																																																																																																																																																										-0x228a) < 1) {
																																																																																																																																																																																										*(undefined2*)(iVar17 + -0x7dbc) = 0;
																																																																																																																																																																																										local_4 = 0;
																																																																																																																																																																																									}
																																																																																																																																																																																									else {
																																																																																																																																																																																										*(undefined2*)(iVar17 + -0x7dba) = 0;
																																																																																																																																																																																										local_4 = 0;
																																																																																																																																																																																									}
																																																																																																																																																																																								}
																																																																																																																																																																																							}
																																																																																																																																																																																						}
																																																																																																																																																																																						else {
																																																																																																																																																																																							if (uVar9 == 0xce) {
																																																																																																																																																																																								*(undefined*)0xddb7 =
																																																																																																																																																																																									*(undefined*)(iVar17 + -0x7da2);
																																																																																																																																																																																								bVar6 = *(byte*)(iVar17 + -0x7da2);
																																																																																																																																																																																								*(byte*)0xddb6 = bVar6;
																																																																																																																																																																																								local_4 = (uint)bVar6;
																																																																																																																																																																																							}
																																																																																																																																																																																							else {
																																																																																																																																																																																								if (uVar9 == 0xcf) {
																																																																																																																																																																																									FUN_20ac_cf64(cVar22, uVar25);
																																																																																																																																																																																									local_4 = uVar9;
																																																																																																																																																																																								}
																																																																																																																																																																																								else {
																																																																																																																																																																																									if (uVar9 == 0xd0) {
																																																																																																																																																																																										FUN_20ac_d3a3(cVar22, uVar25);
																																																																																																																																																																																										local_4 = uVar9;
																																																																																																																																																																																									}
																																																																																																																																																																																									else {
																																																																																																																																																																																										if (uVar9 == 0xd1) {
																																																																																																																																																																																											if ((*(int*)0xd772 < 1) ||
																																																																																																																																																																																												(*(int*)0xd774 < 0x78)) {
																																																																																																																																																																																												*(undefined2*)(iVar17 + -0x7da0) = 1;
																																																																																																																																																																																											}
																																																																																																																																																																																											else {
																																																																																																																																																																																												FUN_3ce4_170a(0x14);
																																																																																																																																																																																												*(uint*)(iVar17 + -0x7da0) = uVar9;
																																																																																																																																																																																											}
																																																																																																																																																																																											FUN_20ac_d306(cVar22, uVar25);
																																																																																																																																																																																											FUN_3ce4_170a(0x14);
																																																																																																																																																																																											local_4 = uVar9;
																																																																																																																																																																																											if (uVar9 == 0) {
																																																																																																																																																																																												FUN_3ce4_170a(3);
																																																																																																																																																																																												*(undefined*)(iVar17 + -0x7d90) =
																																																																																																																																																																																													(char)(uVar9 + 3);
																																																																																																																																																																																												local_4 = uVar9 + 3;
																																																																																																																																																																																											}
																																																																																																																																																																																										}
																																																																																																																																																																																										else {
																																																																																																																																																																																											if (uVar9 == 0xd2) {
																																																																																																																																																																																												FUN_2e48_2d63(uVar25);
																																																																																																																																																																																												local_4 = uVar9;
																																																																																																																																																																																												if (((char)uVar9 != '\0') &&
																																																																																																																																																																																													(FUN_2e48_2b33(uVar25),
																																																																																																																																																																																														local_4 = uVar9, (int)uVar9 < 2)) {
																																																																																																																																																																																													bVar6 = *(byte*)0x4cd;
																																																																																																																																																																																													local_4 = uVar9 & 0xff00 |
																																																																																																																																																																																														(uint)bVar6;
																																																																																																																																																																																													FUN_20ac_c302(bVar6, uVar25);
																																																																																																																																																																																												}
																																																																																																																																																																																											}
																																																																																																																																																																																											else {
																																																																																																																																																																																												if (uVar9 == 0xd4) {
																																																																																																																																																																																													FUN_20ac_d78e(cVar22, uVar25);
																																																																																																																																																																																													local_4 = uVar9;
																																																																																																																																																																																												}
																																																																																																																																																																																												else {
																																																																																																																																																																																													if (uVar9 == 0xff) {
																																																																																																																																																																																														piVar1 = (int*)(iVar17 + -0x7da0)
																																																																																																																																																																																															;
																																																																																																																																																																																														*piVar1 = *piVar1 + param_1;
																																																																																																																																																																																														local_4 = *(uint*)(iVar17 +
																																																																																																																																																																																															-0x7da0);
																																																																																																																																																																																														if (*(int*)(iVar17 + -0x7da2) < (int)local_4) {
																																																																																																																																																																																															FUN_1000_0f13((char)*(undefined2*)
																																																																																																																																																																																																(iVar17 + -0x7d9a),
																																																																																																																																																																																																(char)*(undefined2*)
																																																																																																																																																																																																(iVar17 + -0x7d96));
																																																																																																																																																																																															piVar1 = (int*)(iVar17 + -0x7d96);
																																																																																																																																																																																															*piVar1 = *piVar1 + 1;
																																																																																																																																																																																															local_4 = *(uint*)(iVar17 + -0x7da2);
																																																																																																																																																																																															piVar1 = (int*)(iVar17 + -0x7da0);
																																																																																																																																																																																															*piVar1 = *piVar1 - local_4;
																																																																																																																																																																																														}
																																																																																																																																																																																													}
																																																																																																																																																																																													else {
																																																																																																																																																																																														if (uVar9 == 300) {
																																																																																																																																																																																															FUN_20ac_29f5(uVar25);
																																																																																																																																																																																															local_4 = uVar9;
																																																																																																																																																																																														}
																																																																																																																																																																																													}
																																																																																																																																																																																												}
																																																																																																																																																																																											}
																																																																																																																																																																																										}
																																																																																																																																																																																									}
																																																																																																																																																																																								}
																																																																																																																																																																																							}
																																																																																																																																																																																						}
																																																																																																																																																																																					}
																																																																																																																																																																																				}
																																																																																																																																																																																			}
																																																																																																																																																																																		}
																																																																																																																																																																																	}
																																																																																																																																																																																}
																																																																																																																																																																															}
																																																																																																																																																																														}
																																																																																																																																																																													}
																																																																																																																																																																												}
																																																																																																																																																																											}
																																																																																																																																																																										}
																																																																																																																																																																									}
																																																																																																																																																																								}
																																																																																																																																																																							}
																																																																																																																																																																						}
																																																																																																																																																																					}
																																																																																																																																																																				}
																																																																																																																																																																			}
																																																																																																																																																																		}
																																																																																																																																																																	}
																																																																																																																																																																}
																																																																																																																																																															}
																																																																																																																																																														}
																																																																																																																																																													}
																																																																																																																																																												}
																																																																																																																																																											}
																																																																																																																																																										}
																																																																																																																																																									}
																																																																																																																																																								}
																																																																																																																																																							}
																																																																																																																																																						}
																																																																																																																																																					}
																																																																																																																																																				}
																																																																																																																																																			}
																																																																																																																																																		}
																																																																																																																																																	}
																																																																																																																																																}
																																																																																																																																															}
																																																																																																																																														}
																																																																																																																																													}
																																																																																																																																												}
																																																																																																																																											}
																																																																																																																																										}
																																																																																																																																									}
																																																																																																																																								}
																																																																																																																																							}
																																																																																																																																						}
																																																																																																																																					}
																																																																																																																																				}
																																																																																																																																			}
																																																																																																																																		}
																																																																																																																																	}
																																																																																																																																}
																																																																																																																															}
																																																																																																																														}
																																																																																																																													}
																																																																																																																												}
																																																																																																																											}
																																																																																																																										}
																																																																																																																									}
																																																																																																																								}
																																																																																																																							}
																																																																																																																						}
																																																																																																																					}
																																																																																																																				}
																																																																																																																			}
																																																																																																																		}
																																																																																																																	}
																																																																																																																}
																																																																																																															}
																																																																																																														}
																																																																																																													}
																																																																																																												}
																																																																																																											}
																																																																																																										}
																																																																																																									}
																																																																																																								}
																																																																																																							}
																																																																																																						}
																																																																																																					}
																																																																																																				}
																																																																																																			}
																																																																																																		}
																																																																																																	}
																																																																																																}
																																																																																															}
																																																																																														}
																																																																																													}
																																																																																												}
																																																																																											}
																																																																																										}
																																																																																									}
																																																																																								}
																																																																																							}
																																																																																						}
																																																																																					}
																																																																																				}
																																																																																			}
																																																																																		}
																																																																																	}
																																																																																}
																																																																															}
																																																																														}
																																																																													}
																																																																												}
																																																																											}
																																																																										}
																																																																									}
																																																																								}
																																																																							}
																																																																						}
																																																																					}
																																																																				}
																																																																			}
																																																																		}
																																																																	}
																																																																}
																																																															}
																																																														}
																																																													}
																																																												}
																																																											}
																																																										}
																																																									}
																																																								}
																																																							}
																																																						}
																																																					}
																																																				}
																																																			}
																																																		}
																																																	}
																																																}
																																															}
																																														}
																																													}
																																												}
																																											}
																																										}
																																									}
																																								}
																																							}
																																						}
																																					}
																																				}
																																			}
																																		}
																																	}
																																}
																															}
																														}
																													}
																												}
																											}
																										}
																									}
																								}
																							}
																						}
																					}
																				}
																			}
																		}
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			FUN_2e48_3f56(uVar25, 1);
			*(uint*)(iVar17 + -0x7d94) = local_4;
			local_a = *(int*)(iVar17 + -0x7db8);
			local_e = *(uint*)(iVar17 + -0x7db4);
			bVar6 = *(byte*)(iVar17 + -0x7dc0);
			uVar9 = local_e & 0xff00 | (uint)bVar6;
			if (bVar6 == 1) {
				FUN_2e48_0a6c(0x19, (char)*(undefined2*)0xd4d2, (char)*(undefined2*)(iVar17 + -0x7db8));
				local_a = uVar9 + *(int*)(iVar17 + -0x7db8);
				iVar18 = local_a;
				FUN_2e48_0a6c(0x19, (char)*(undefined2*)0xd4d6, (char)*(undefined2*)(iVar17 + -0x7db4));
				local_e = iVar18 + *(int*)(iVar17 + -0x7db4);
			}
			else {
				if (bVar6 == 3) {
					FUN_2e48_0a6c(0x50, (char)*(undefined2*)0xd4d2, (char)*(undefined2*)(iVar17 + -0x7db8));
					local_a = uVar9 + *(int*)(iVar17 + -0x7db8);
					iVar18 = local_a;
					FUN_2e48_0a6c(0x50, (char)*(undefined2*)0xd4d6, (char)*(undefined2*)(iVar17 + -0x7db4));
					local_e = iVar18 + *(int*)(iVar17 + -0x7db4);
				}
				else {
					if (bVar6 == 4) {
						FUN_2e48_0a6c(0x14, (char)*(undefined2*)0xd4d2, (char)*(undefined2*)(iVar17 + -0x7db8));
						local_a = *(int*)(iVar17 + -0x7db8) - uVar9;
						iVar18 = local_a;
						FUN_2e48_0a6c(0x14, (char)*(undefined2*)0xd4d6, (char)*(undefined2*)(iVar17 + -0x7db4));
						local_e = *(int*)(iVar17 + -0x7db4) - iVar18;
					}
					else {
						if (bVar6 == 0xb) {
							FUN_2e48_0a6c(0x28, (char)*(undefined2*)0xd4d2, (char)*(undefined2*)(iVar17 + -0x7db8)
							);
							local_a = uVar9 + *(int*)(iVar17 + -0x7db8);
							iVar18 = local_a;
							FUN_2e48_0a6c(0x28, (char)*(undefined2*)0xd4d6, (char)*(undefined2*)(iVar17 + -0x7db4)
							);
							local_e = iVar18 + *(int*)(iVar17 + -0x7db4);
						}
						else {
							if (bVar6 == 5) {
								if (*(int*)(iVar17 + -0x7d94) < 0x3c) {
									local_a = *(int*)0xd4d2;
									local_e = *(uint*)0xd4d6;
								}
							}
							else {
								if (bVar6 == 6) {
									piVar1 = (int*)(iVar17 + -0x7d94);
									*piVar1 = *piVar1 + 100;
								}
								else {
									if (bVar6 == 7) {
										piVar1 = (int*)(iVar17 + -0x7d94);
										*piVar1 = *piVar1 + -100;
									}
									else {
										if (bVar6 == 8) {
											FUN_2e48_0a6c(0x14, (char)*(undefined2*)0xd4d2,
												(char)*(undefined2*)(iVar17 + -0x7db8));
											local_a = uVar9 + *(int*)(iVar17 + -0x7db8);
											iVar18 = local_a;
											FUN_2e48_0a6c(0x14, (char)*(undefined2*)0xd4d6,
												(char)*(undefined2*)(iVar17 + -0x7db4));
											local_e = iVar18 + *(int*)(iVar17 + -0x7db4);
											piVar1 = (int*)(iVar17 + -0x7d94);
											*piVar1 = *piVar1 + -100;
										}
										else {
											if (bVar6 == 9) {
												uVar9 = (*(int*)0xd4d4 - *(int*)(iVar17 + -0x7db6)) - *(int*)0xd4b2;
												uVar8 = (int)uVar9 >> 0xf;
												piVar1 = (int*)(iVar17 + -0x7d94);
												*piVar1 = *piVar1 + ((uVar9 ^ uVar8) - uVar8) + 100;
											}
											else {
												if (bVar6 == 10) {
													if (*(int*)0xd4d4 + 10 < *(int*)(iVar17 + -0x7db6)) {
														local_a = 10000;
													}
												}
												else {
													if (((bVar6 == 0x14) && (0 < *(int*)(iVar17 + -0x7d9c))) &&
														(0 < *(int*)(iVar17 + -0x7d96))) {
														iVar18 = *(int*)(iVar17 + -0x7d96) * 0x36;
														if (*(char*)0xd502 == '\0') {
															local_a = 10000;
														}
														if (*(int*)(iVar18 + -0x7dba) == 0) {
															*(undefined2*)(local_6 * 0x36 + -0x7dba) = 0;
														}
														else {
															if (0x1e < *(byte*)CONCAT22(unaff_DS, iVar18 + -0x7dc4)) {
																uVar4 = *(undefined4*)
																	((*(byte*)CONCAT22(unaff_DS, iVar18 + -0x7dc4) - 0x1e) *
																		0x1a4 + (uint) * (byte*)(iVar18 + -0x7dc3) * 0x3c +
																		(uint) * (byte*)(iVar18 + -0x7dc1) * 0xc + 0x6e84);
																*(undefined*)
																	((int)uVar4 + *(int*)(local_6 * 0x36 + -0x7d9c) * 6 + -1) = uVar25
																	;
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			uVar9 = *(int*)0xd4d2 / 100;
			bStack69 = (char)uVar9 >> 7;
			uVar8 = (int)(char)(local_a / 100) - (uVar9 & 0xff | (uint)bStack69 << 8);
			uVar9 = *(int*)0xd4d6 / 100;
			bStack69 = (char)uVar9 >> 7;
			uVar9 = (int)(char)((int)local_e / 100) - (uVar9 & 0xff | (uint)bStack69 << 8);
			if (*(byte*)(iVar17 + -0x7dc0) < 200) {
				uVar14 = (int)uVar8 >> 0xf;
				uVar10 = (uVar8 ^ uVar14) - uVar14;
				uVar15 = (int)uVar9 >> 0xf;
				uVar16 = uVar14;
				if (((int)(uint) * (byte*)0x3117 < (int)uVar10) ||
					(uVar10 = (uVar9 ^ uVar15) - uVar15, uVar16 = uVar15,
						(int)(uint) * (byte*)0x3117 < (int)uVar10)) {
					in_EDX = (ulong)uVar16;
					in_EAX = (ulong)uVar10;
					if (99 < *(byte*)(iVar17 + -0x7dc0)) {
						in_EDX = (ulong)uVar14;
						uVar9 = ((uVar8 ^ uVar14) - uVar14) + ((uVar9 ^ uVar15) - uVar15);
						in_EAX = (ulong)uVar9;
						if ((int)uVar9 < (int)(*(byte*)(iVar17 + -0x7dc0) - 0x5e)) {
							FUN_2e48_2ec4(0xf6, unaff_SS,
								(char)*(undefined2*)(iVar17 + -0x7db4) - (char)*(undefined2*)0xd4d6,
								(char)*(undefined2*)(iVar17 + -0x7db8) - (char)*(undefined2*)0xd4d2);
							uVar9 = (uVar8 + *(int*)0xd4d0 & 0x1ff) - 0x100;
							uVar8 = (int)uVar9 >> 0xf;
							in_EDX = (ulong)uVar8;
							uVar8 = (uVar9 ^ uVar8) - uVar8;
							in_EAX = (ulong)uVar8;
							if ((int)uVar8 < 100) {
								FUN_1000_100b(0xfe, 0xb0, uVar24, uVar25);
							}
						}
					}
				}
				else {
					uVar9 = (uVar9 + 10) * 0x18;
					in_EDX = (ulong)uVar9;
					in_EAX = (ulong)(uVar8 + 10);
					FUN_1000_100b(0xfe, (char)(uVar8 + 10) * -8 + (char)uVar9 + -0x1d, uVar24, uVar25);
				}
			}
			else {
				FUN_2e48_2ec4(0xf6, unaff_SS,
					(char)*(undefined2*)(iVar17 + -0x7db4) - (char)*(undefined2*)0xd4d6,
					(char)*(undefined2*)(iVar17 + -0x7db8) - (char)*(undefined2*)0xd4d2);
				uVar9 = (uVar8 + *(int*)0xd4d0 & 0x1ff) - 0x100;
				uVar8 = (int)uVar9 >> 0xf;
				in_EDX = (ulong)uVar8;
				uVar8 = (uVar9 ^ uVar8) - uVar8;
				in_EAX = (ulong)uVar8;
				if ((int)uVar8 < 100) {
					FUN_1000_100b(0xfe, 0xb0, uVar24, uVar25);
				}
			}
		}
		if (local_6 == 0xa4) break;
		local_6 = local_6 + 1;
	}
	return;
}
*/
}